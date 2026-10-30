#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/atomic.h>
#include <linux/string.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Embedded Linux Lab");
MODULE_DESCRIPTION("LAB-02: Simple Character Device Driver");
#define DRIVER_VERSION "1.0"
MODULE_VERSION(DRIVER_VERSION);

#define DEVICE_NAME     "lab2"
#define CLASS_NAME      "lab2_class"
#define BUFFER_SIZE     1024
#define MAX_WRITE_SIZE  128
#define DRIVER_MAJOR    240
/* 240-254: reserved for local/experimental use */



static int      major_number = DRIVER_MAJOR;
static char     device_buffer[BUFFER_SIZE];
static int      buffer_len = 0;
static int      open_count = 0;
static atomic64_t read_count = ATOMIC64_INIT(0);
static atomic64_t total_bytes_read = ATOMIC64_INIT(0);
static atomic64_t write_count = ATOMIC64_INIT(0);
static atomic64_t total_bytes_written = ATOMIC64_INIT(0);
static struct timespec64 last_write_time;
static bool has_last_write_time;
static struct class  *lab2_class = NULL;
static struct device *lab2_device = NULL;

static DEFINE_MUTEX(lab2_mutex);
/* ===== PROCFS Implementation ===== */
static struct proc_dir_entry *proc_entry;

static int lab2_proc_show(struct seq_file *m, void *v) {
    (void)v;
    mutex_lock(&lab2_mutex); // Mot anh chup nhat quan, tranh doc buffer khi write dang sua
    seq_puts(m, "LAB-02 Driver Statistics\n========================\n");
    seq_printf(m, "Driver name    : %s\nMajor number   : %d\n", DEVICE_NAME, major_number);
    seq_printf(m, "Buffer size    : %d bytes\nData length    : %d bytes\n", BUFFER_SIZE, buffer_len);
    seq_printf(m, "Open count     : %d\n", open_count);
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
    return single_open(file, lab2_proc_show, NULL);
}

static const struct proc_ops lab2_proc_fops = {
    .proc_open    = lab2_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
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
    return sysfs_emit(buf, "write reset to reset statistics\n"); // Huong dan, khong sua thong ke khi doc
}
static ssize_t reset_stats_store(struct device *dev, struct device_attribute *attr,
                                 const char *buf, size_t count) {
    (void)dev; (void)attr;
    if (!sysfs_streq(buf, "reset")) return -EINVAL; // Chap nhan reset hoac reset\n
    mutex_lock(&lab2_mutex); // Reset cac bo dem trong cung mot vung khoa
    open_count = 0;
    atomic64_set(&read_count, 0); atomic64_set(&write_count, 0);
    atomic64_set(&total_bytes_read, 0); atomic64_set(&total_bytes_written, 0);
    mutex_unlock(&lab2_mutex);
    return count; // Thanh cong: tra so byte cua lenh sysfs
}

static DEVICE_ATTR_RW(reset_stats);
static DEVICE_ATTR_RO(read_count);
static DEVICE_ATTR_RO(version);
static DEVICE_ATTR_RO(status);
static DEVICE_ATTR_RO(buffer_size);
static DEVICE_ATTR_RO(buffer_len);
static DEVICE_ATTR_RO(open_count);
static DEVICE_ATTR_RO(last_data);

static struct attribute *lab2_attrs[] = {
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
static int lab2_open(struct inode *inode, struct file *file) {
    int count;
    (void)inode; (void)file;
    mutex_lock(&lab2_mutex);
    if (open_count == INT_MAX) { mutex_unlock(&lab2_mutex); return -EOVERFLOW; }
    count = ++open_count; // Chi dem lan open thanh cong; khong tang neu tra loi
    mutex_unlock(&lab2_mutex);
    pr_info("lab2_driver: device opened (count=%d)\n", count); // Log ban sao, khong doc bien chung sau unlock
    return 0;
}
static int lab2_release(struct inode *inode, struct file *file) {
    pr_info("lab2_driver: device closed\n");
    return 0;
}
static ssize_t lab2_read(struct file *file, char __user *buf, size_t len, loff_t *offset) {
    ssize_t result;
    (void)file;
    mutex_lock(&lab2_mutex); // Khoa ca kiem tra do dai, copy du lieu va cap nhat thong ke
    atomic64_inc(&read_count); // Giu quy uoc bai 1: dem ca EOF va loi
    if (*offset < 0) { result = -EINVAL; goto out; }
    if (*offset >= buffer_len || len == 0) { result = 0; goto out; }
    if (len > buffer_len - *offset) len = buffer_len - *offset;
    if (copy_to_user(buf, device_buffer + *offset, len)) { result = -EFAULT; goto out; }
    *offset += len; // Chi tang vi tri sau khi copy thanh cong
    atomic64_add(len, &total_bytes_read);
    result = len;
out:
    mutex_unlock(&lab2_mutex); // Tat ca nhanh deu nha khoa
    return result;
}

static bool lab2_is_clear(const char *data, size_t len) {
    return (len == 5 && !memcmp(data, "clear", 5)) ||
           (len == 6 && !memcmp(data, "clear\n", 6)); // So dung ca do dai va noi dung, khong nhan clearX
}

static ssize_t lab2_write(struct file *file, const char __user *buf, size_t len, loff_t *offset) {
    char *temp;
    size_t requested = len; // Do dai goc, truoc khi cat xuong gioi han ghi
    (void)file; (void)offset;
    if (!len) return 0;
    if (len > MAX_WRITE_SIZE) len = MAX_WRITE_SIZE; // Giu gioi han 128 byte moi lan ghi cua bai 4
    temp = memdup_user(buf, len); // Copy vao dem rieng: EFAULT khong lam hong du lieu cu
    if (IS_ERR(temp)) return PTR_ERR(temp);
    mutex_lock(&lab2_mutex);
    if (lab2_is_clear(temp, requested)) {
        memset(device_buffer, 0, sizeof(device_buffer)); buffer_len = 0;
    } else {
        if (len > BUFFER_SIZE - 1 - buffer_len) {
            mutex_unlock(&lab2_mutex); kfree(temp); return -ENOSPC; // Khong du cho thi giu nguyen buffer
        }
        memcpy(device_buffer + buffer_len, temp, len); // Noi du lieu moi vao cuoi du lieu cu
        buffer_len += len; device_buffer[buffer_len] = '\0';
    }
    atomic64_inc(&write_count); atomic64_add(len, &total_bytes_written);
    ktime_get_real_ts64(&last_write_time); has_last_write_time = true;
    mutex_unlock(&lab2_mutex);
    kfree(temp);
    return len;
}

static struct file_operations lab2_fops = {
    .owner = THIS_MODULE,
    .open = lab2_open,
    .release = lab2_release,
    .read = lab2_read,
    .write = lab2_write,
};
static int __init lab2_init(void) {
    int ret;

    pr_info("lab2_driver: initializing module\n");

    /* Đăng ký character device với major number cố định */
    ret = register_chrdev(major_number, DEVICE_NAME, &lab2_fops);
    if (ret < 0) {
        pr_err("lab2_driver: register_chrdev failed: %d\n", ret);
        return ret;
    }

    /* Tạo device class và device node tự động trong /sys */
    lab2_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(lab2_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_class);
    }

    lab2_device = device_create(lab2_class, NULL,
                                MKDEV(major_number, 0),
                                NULL, DEVICE_NAME);
    if (IS_ERR(lab2_device)) {
        class_destroy(lab2_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        return PTR_ERR(lab2_device);
    }

/* Đăng ký sysfs attribute group */
ret = sysfs_create_group(&lab2_device->kobj, &lab2_attr_group);
if (ret)
    pr_warn("lab2_driver: sysfs_create_group failed\n");

    pr_info("lab2_driver: loaded, major=%d\n", major_number);
proc_entry = proc_create("lab2_info", 0444, NULL, &lab2_proc_fops);
if (!proc_entry)
    pr_warn("lab2_driver: failed to create /proc/lab2_info\n");
else
    pr_info("lab2_driver: /proc/lab2_info created\n");
    return 0;
}
static void __exit lab2_exit(void) {
 if (proc_entry)
        proc_remove(proc_entry);
   sysfs_remove_group(&lab2_device->kobj, &lab2_attr_group);
    device_destroy(lab2_class, MKDEV(major_number, 0));
    class_destroy(lab2_class);
    unregister_chrdev(major_number, DEVICE_NAME);
    pr_info("lab2_driver: module unloaded\n");
}

module_init(lab2_init);
module_exit(lab2_exit);
