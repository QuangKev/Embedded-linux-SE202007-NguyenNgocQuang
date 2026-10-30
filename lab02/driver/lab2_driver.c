// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>                                                                               // Khai bao module, THIS_MODULE ngan go module khi fd van dang mo
#include <linux/kernel.h>                                                                               // pr_info, sysfs_emit va cac ham ho tro kernel
#include <linux/fs.h>                                                                                   // struct file, file_operations, nonseekable_open
#include <linux/uaccess.h>                                                                              // Copy giua user-space va kernel-space, khong dereference con tro user truc tiep
#include <linux/cdev.h>                                                                                 // Kieu character device cua driver nen
#include <linux/device.h>                                                                               // class_create/device_create va sysfs DEVICE_ATTR
#include <linux/proc_fs.h>                                                                              // Tao/xoa /proc/lab2_info
#include <linux/seq_file.h>                                                                             // seq_printf, single_open: xuat bao cao text qua procfs
#include <linux/slab.h>                                                                                 // kzalloc/kfree, memdup_user: cap dem va copy user an toan
#include <linux/mutex.h>                                                                                // Mutex bao ve moi du lieu chung co the thay doi
#include <linux/atomic.h>                                                                               // Giu cac bo dem atomic64 tu bai 1-10; van khoa mutex de reset nhat quan
#include <linux/string.h>                                                                               // memcpy, memset, memcmp, sysfs_streq
#include <linux/timekeeping.h>                                                                          // ktime_get_real_ts64: thoi diem ghi theo Unix epoch
#include <linux/version.h>                                                                              // Ho tro class_create cua kernel 5.15 va kernel moi
#include <linux/compat.h>                                                                               // compat_ptr_ioctl cho app 32-bit tren kernel 64-bit
#include "lab2_ioctl.h"                                                                                 // Ma ioctl va kieu __u32 dung chung voi app

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
#define DRIVER_VERSION "1.0"
MODULE_VERSION(DRIVER_VERSION);

#define DEVICE_NAME     "lab2"                                                                          // Giu ten device /dev/lab2 cua bai nen
#define CLASS_NAME      "lab2_class"                                                                    // Giu duong dan /sys/class/lab2_class/lab2
#define BUFFER_SIZE     1024                                                                            // Tong suc chua, du lieu toi da 1023 byte + NUL
#define MAX_WRITE_SIZE  128                                                                             // Giu tran 128 byte moi write cua bai 4
#define DRIVER_MAJOR    240                                                                             // Giu major 240, minor 0
/* 240-254: reserved for local/experimental use */

static int      major_number = DRIVER_MAJOR;
static char     device_buffer[BUFFER_SIZE];                                                             // Dem dung chung giua cac fd, khong phai buffer vong
static int      buffer_len = 0;                                                                         // So byte hop le dang co; doc/sua duoi lab2_mutex
static unsigned int last_write_size;                                                                    // Byte cua write thanh cong gan nhat, duoi lab2_mutex
static unsigned int max_size = MAX_WRITE_SIZE;                                                          // Gioi han moi write, mac dinh 128; bao ve boi lab2_mutex
static int      open_count = 0;                                                                         // Tong open thanh cong ke tu insmod hoac reset
static atomic64_t read_count = ATOMIC64_INIT(0);
static atomic64_t total_bytes_read = ATOMIC64_INIT(0);
static atomic64_t write_count = ATOMIC64_INIT(0);
static atomic64_t total_bytes_written = ATOMIC64_INIT(0);
static struct timespec64 last_write_time;                                                               // Thoi diem write thanh cong gan nhat, khong phai thoi diem ioctl
static bool has_last_write_time;                                                                        // Phan biet chua ghi lan nao voi timestamp bang 0
static struct class  *lab2_class = NULL;
static struct device *lab2_device = NULL;

static DEFINE_MUTEX(lab2_mutex);                                                                        // Chi mot luong giu khoa, moi nhanh loi phai unlock
static u64 data_generation;                                                                             // Doi sau write/clear; mutex bao ve ca tren ARM 32-bit
struct lab2_reader { u64 generation; };                                                                 // Moi open co rieng, truy cap khi giu lab2_mutex
/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v) {                                                // Bao cao thong ke, giu khoa de cac gia tri cung mot thoi diem
    (void)v;
    mutex_lock(&lab2_mutex);                                                                            // Mot anh chup nhat quan, tranh doc buffer khi write dang sua
    seq_puts(m, "LAB-02 Driver Statistics\n========================\n");
    seq_printf(m, "Driver name    : %s\nMajor number   : %d\n", DEVICE_NAME, major_number);
    seq_printf(m, "Buffer size    : %d bytes\nData length    : %d bytes\n", BUFFER_SIZE, buffer_len);
    seq_printf(m, "Open count     : %d\n", open_count);
    seq_printf(m, "Read count     : %lld\n", (long long)atomic64_read(&read_count));
    seq_printf(m, "Status         : %s\n", buffer_len ? "DATA" : "EMPTY");
    seq_printf(m, "Max write size : %u\nLast write size: %u\n", max_size, last_write_size);
    seq_printf(m, "Write count    : %lld\n", (long long)atomic64_read(&write_count));
    seq_printf(m, "Total bytes read: %lld\n", (long long)atomic64_read(&total_bytes_read));
    seq_printf(m, "Total bytes written: %lld\n", (long long)atomic64_read(&total_bytes_written));
    if (has_last_write_time)
        seq_printf(m, "Last write time: %lld.%09lu seconds since Unix epoch (UTC)\n",
                   (long long)last_write_time.tv_sec, (unsigned long)last_write_time.tv_nsec);
    else seq_puts(m, "Last write time: never\n");
    if (buffer_len > 0) seq_printf(m, "Last data      : [%s]\n", device_buffer);
    mutex_unlock(&lab2_mutex);
    return 0;
}

static int lab2_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, lab2_proc_show, NULL);                                                     // seq_file quan ly dem text va EOF cho procfs
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,                                                                           // Doc text duoc tao boi lab2_proc_show
    .proc_lseek   = seq_lseek,                                                                          // Seek trong bao cao procfs, khong phai seek tren /dev/lab2
    .proc_release = single_release,                                                                     // Tra lai tai nguyen single_open
};
/* ===== SYSFS Implementation ===== */
static ssize_t version_show(struct device *dev,
                            struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%s\n", DRIVER_VERSION);
}

static ssize_t status_show(struct device *dev,
                           struct device_attribute *attr, char *buf)
{
    ssize_t ret;

    mutex_lock(&lab2_mutex);
    ret = sysfs_emit(buf, "%s\n", buffer_len == 0 ? "EMPTY" : "DATA");
    mutex_unlock(&lab2_mutex);

    return ret;
}

static ssize_t buffer_size_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    return sysfs_emit(buf, "%d\n", BUFFER_SIZE);
}

static ssize_t buffer_len_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex); n = sysfs_emit(buf, "%d\n", buffer_len); mutex_unlock(&lab2_mutex);
    return n;
}

static ssize_t open_count_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex); n = sysfs_emit(buf, "%d\n", open_count); mutex_unlock(&lab2_mutex);
    return n;
}

static ssize_t last_data_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex);
    n = sysfs_emit(buf, "%s\n", buffer_len > 0 ? device_buffer : "(empty)");
    mutex_unlock(&lab2_mutex);
    return n;
}

static ssize_t read_count_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex); n = sysfs_emit(buf, "%lld\n", (long long)atomic64_read(&read_count));
    mutex_unlock(&lab2_mutex);
    return n;
}

static ssize_t reset_stats_show(struct device *dev, struct device_attribute *attr, char *buf) {
    (void)dev; (void)attr;
    return sysfs_emit(buf, "write reset to reset statistics\n");                                        // Huong dan, khong sua thong ke khi doc
}
static ssize_t reset_stats_store(struct device *dev, struct device_attribute *attr,
                                 const char *buf, size_t count) {
    (void)dev; (void)attr;
    if (!sysfs_streq(buf, "reset")) return -EINVAL;                                                     // Chap nhan reset hoac reset\n
    mutex_lock(&lab2_mutex);                                                                            // Reset cac bo dem trong cung mot vung khoa
    open_count = 0;
    atomic64_set(&read_count, 0); atomic64_set(&write_count, 0);
    atomic64_set(&total_bytes_read, 0); atomic64_set(&total_bytes_written, 0);
    mutex_unlock(&lab2_mutex);
    return count;                                                                                       // Thanh cong: tra so byte cua lenh sysfs
}

static DEVICE_ATTR_RW(reset_stats);
static ssize_t max_size_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex); n = sysfs_emit(buf, "%u\n", max_size); mutex_unlock(&lab2_mutex);
    return n;
}
static ssize_t max_size_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count) {
    unsigned int value; (void)dev; (void)attr;
    if (kstrtouint(buf, 10, &value) || value < 1 || value > MAX_WRITE_SIZE) return -EINVAL;
    mutex_lock(&lab2_mutex); max_size = value; mutex_unlock(&lab2_mutex);                               // Khong cat du lieu dang luu
    return count;
}
static DEVICE_ATTR_RW(max_size);
static ssize_t last_write_size_show(struct device *dev, struct device_attribute *attr, char *buf) {
    ssize_t n; (void)dev; (void)attr;
    mutex_lock(&lab2_mutex); n = sysfs_emit(buf, "%u\n", last_write_size); mutex_unlock(&lab2_mutex);
    return n;
}
static DEVICE_ATTR_RO(last_write_size);
static DEVICE_ATTR_RO(read_count);
static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(buffer_size);
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
    &dev_attr_last_write_size.attr,
    &dev_attr_max_size.attr,
    &dev_attr_reset_stats.attr,
    &dev_attr_version.attr,
    &dev_attr_status.attr,
    &dev_attr_buffer_size.attr,
    &dev_attr_read_count.attr,
    &dev_attr_buffer_len.attr,
    &dev_attr_open_count.attr,
    &dev_attr_last_data.attr,
    NULL,
};

static struct attribute_group lab2_attr_group = {
    .attrs = lab2_attrs,
};
static int lab2_open(struct inode *inode, struct file *file) {                                          // Tao trang thai rieng cho fd va dem open thanh cong
    struct lab2_reader *reader;
    int count, ret;
    reader = kzalloc(sizeof(*reader), GFP_KERNEL);                                                      // Bo nho rieng cho moi fd, tra lai trong release
    if (!reader) return -ENOMEM;                                                                        // Open that bai khong tang open_count
    ret = nonseekable_open(inode, file);                                                                // Bai nay doc tuan tu, khong them seek ngoai de
    if (ret) { kfree(reader); return ret; }
    file->f_mode |= FMODE_ATOMIC_POS;                                                                    // Kernel khoa f_pos khi nhieu thread dung chung mot fd
    mutex_lock(&lab2_mutex);
    if (open_count == INT_MAX) { mutex_unlock(&lab2_mutex); kfree(reader); return -EOVERFLOW; }
    reader->generation = data_generation; file->private_data = reader;
    count = ++open_count;                                                                               // Tang sau khi moi buoc co the loi da thanh cong
    mutex_unlock(&lab2_mutex);
    pr_info("lab2_driver: device opened (count=%d)\n", count);
    return 0;
}
static int lab2_release(struct inode *inode, struct file *file) {                                       // Giai phong trang thai rieng cua fd
    (void)inode;
    kfree(file->private_data);                                                                          // Kernel goi release khi khong con nguoi dung file nay
    pr_info("lab2_driver: device closed\n");
    return 0;
}
static ssize_t lab2_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {             // Doc tuan tu, quan ly EOF va phien ban du lieu
    struct lab2_reader *reader = file->private_data;
    loff_t pos;
    ssize_t result;
    mutex_lock(&lab2_mutex);                                                                            // Bao ve buffer, thong ke va generation trong cung khoa
    atomic64_inc(&read_count);                                                                          // Dem ca EOF va loi theo quy uoc bai 1
    if (*offset < 0) { result = -EINVAL; goto out; }
    if (!len) { result = 0; goto out; }
    pos = reader->generation == data_generation ? *offset : 0;                                          // Write moi: fd duoc doc tu dau
    if (pos >= buffer_len) { result = 0; goto completed; }                                              // EOF, cat dung binh thuong
    if (len > buffer_len - pos) len = buffer_len - pos;
    if (copy_to_user(buf, device_buffer + pos, len)) { result = -EFAULT; goto out; }
    pos += len; atomic64_add(len, &total_bytes_read);                                                   // Chi dem byte tra thanh cong
    result = len;
completed:
    *offset = pos; reader->generation = data_generation;                                                // Khong doi offset neu copy loi
out:
    mutex_unlock(&lab2_mutex);
    return result;
}

static bool lab2_is_clear(const char *data, size_t len) {
    return (len == 5 && !memcmp(data, "clear", 5)) ||
           (len == 6 && !memcmp(data, "clear\n", 6));                                                   // So dung ca do dai va noi dung, khong nhan clearX
}

static ssize_t lab2_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {      // Ghi noi du lieu hoac nhan lenh clear
    char *temp;
    size_t requested = len;                                                                             // Do dai goc, truoc khi cat xuong gioi han ghi
    (void)file; (void)offset;
    if (!len) return 0;
    if (len > MAX_WRITE_SIZE) len = MAX_WRITE_SIZE;                                                     // Giu gioi han 128 byte moi lan ghi cua bai 4
    temp = memdup_user(buf, len);                                                                       // Copy vao dem rieng: EFAULT khong lam hong du lieu cu
    if (IS_ERR(temp)) return PTR_ERR(temp);
    mutex_lock(&lab2_mutex);
    if (lab2_is_clear(temp, requested)) {
        memset(device_buffer, 0, sizeof(device_buffer)); buffer_len = 0;
    } else {
        if (len > max_size) len = max_size;                                                             // Chi gioi han du lieu; lenh clear khong bi cat
        if (len > BUFFER_SIZE - 1 - buffer_len) {
            mutex_unlock(&lab2_mutex); kfree(temp); return -ENOSPC;                                     // Khong du cho thi giu nguyen buffer
        }
        memcpy(device_buffer + buffer_len, temp, len);                                                  // Noi du lieu moi vao cuoi du lieu cu
        buffer_len += len; device_buffer[buffer_len] = '\0';
    }
    data_generation++;                                                                                  // Danh dau noi dung moi de cac fd doc lai tu dau
    last_write_size = len;                                                                              // Khong cap nhat neu zero-write, copy loi hay ENOSPC
    atomic64_inc(&write_count); atomic64_add(len, &total_bytes_written);
    ktime_get_real_ts64(&last_write_time); has_last_write_time = true;
    mutex_unlock(&lab2_mutex);
    kfree(temp);
    return len;
}

static long lab2_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {                        // Hai lenh dieu khien khong can parse text
    __u32 length;
    (void)file;
    switch (cmd) {
    case LAB2_GET_BUFFER_LEN:
        mutex_lock(&lab2_mutex); length = buffer_len; mutex_unlock(&lab2_mutex);                        // Chup gia tri duoi khoa
        return copy_to_user((void __user *)arg, &length, sizeof(length)) ? -EFAULT : 0;
    case LAB2_CLEAR_BUFFER:
        mutex_lock(&lab2_mutex);
        memset(device_buffer, 0, sizeof(device_buffer)); buffer_len = 0; data_generation++;
        mutex_unlock(&lab2_mutex);                                                                      // Ioctl khong phai write nen khong tang write_count/last_write_size
        return 0;
    default: return -ENOTTY;                                                                            // Lenh ioctl khong duoc ho tro
    }
}

static struct file_operations lab2_fops = {
    .unlocked_ioctl = lab2_ioctl,                                                                       // Noi ioctl cua userspace voi ham xu ly tren
#ifdef CONFIG_COMPAT
    .compat_ioctl = compat_ptr_ioctl,                                                                   // Du lieu __u32 co cung kich thuoc o 32/64-bit
#endif
    .owner = THIS_MODULE,                                                                               // Kernel giu tham chieu module khi device dang duoc mo
    .open = lab2_open,                                                                                  // open cua userspace goi callback nay
    .release = lab2_release,                                                                            // Dong tham chieu file cuoi cung thi kernel goi release
    .read = lab2_read,                                                                                  // read cua userspace goi callback nay
    .write = lab2_write,                                                                                // write cua userspace goi callback nay
};
static int __init lab2_init(void) {                                                                     // Nap module: tao device, sysfs va proc theo thu tu
    int ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) return ret;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
    lab2_class = class_create(CLASS_NAME);
#else
    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
#endif
    if (IS_ERR(lab2_class)) { ret = PTR_ERR(lab2_class); goto fail_class; }
    lab2_device = device_create(lab2_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) { ret = PTR_ERR(lab2_device); goto fail_device; }
    ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
    if (ret) goto fail_sysfs;
    proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
    if (!proc_entry) { ret = -ENOMEM; goto fail_proc; }
    pr_info("lab2_driver: loaded, major=%d\n", major_number);
    return 0;
fail_proc:
    sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);                                           // Loi thi huy nguoc cac buoc da thanh cong
fail_sysfs:
    device_destroy(lab2_class, MKDEV(major_number, 0));
fail_device:
    class_destroy(lab2_class);
fail_class:
    unregister_chrdev(major_number, DEVICE_NAME);
    return ret;                                                                                         // Khong de module nap nua voi khi tao sysfs/proc that bai
}
static void __exit lab2_exit(void) {                                                                    // Go module: huy du tai nguyen theo thu tu nguoc
    proc_remove(proc_entry);
    sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("lab2_driver: module unloaded\n");
}
module_init(lab2_init);                                                                                 // Dang ky ham kernel goi khi insmod
module_exit(lab2_exit);                                                                                 // Dang ky ham kernel goi khi rmmod
