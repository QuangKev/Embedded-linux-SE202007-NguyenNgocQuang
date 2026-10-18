/*
 * asgn1_driver.c - Character device driver co ioctl interface
 * Author : NguyenNgocQuang - SE202007V
 * Target : ARMv7 Cortex-A9 (QEMU vexpress-a9), Linux 5.15
 * Device : /dev/asgn1 (major 241, minor 0)
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/string.h>
#include "asgn1_ioctl.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NguyenNgocQuang - SE202007V");
MODULE_DESCRIPTION("ASSIGNMENT-01: char device driver with ioctl interface");
MODULE_VERSION("1.0");

#define DEVICE_NAME     "asgn1"
#define CLASS_NAME      "asgn1_class"
#define BUFFER_SIZE     1024
#define DRIVER_MAJOR    241
#define KERNEL_PREFIX   "[KERNEL] "
#define DRIVER_VERSION  "asgn1_driver v1.0 - SE202007V"

/* ===== PHẦN 1: Data layer - trang thai noi bo  ===== */
static char device_buffer[BUFFER_SIZE];
static int  buffer_len;
static int  open_count;
static int  write_count;
static int  read_count;
static int  last_write_size;
static int  mode = ASGN1_MODE_ECHO;

static struct class  *asgn1_class;
static struct device *asgn1_device;

/* Mutex protect tất cả biến trên khi nhiều process truy cập song song.
 * Dùng mutex (không dùng spinlock) vì copy_to/from_user có thể ngủ. */
static DEFINE_MUTEX(asgn1_mutex);

/* ===== PHẦN 2: open / release ===== */

/* asgn1_open - call khi userspace open("/dev/asgn1"). Return: 0 */
static int asgn1_open(struct inode *inode, struct file *file)
{
	mutex_lock(&asgn1_mutex);
	open_count++;
	mutex_unlock(&asgn1_mutex);
	pr_info("asgn1: device opened (open_count=%d)\n", open_count);
	return 0;
}

/* asgn1_release - gọi khi close() fd cuối cùng. Return: 0 */
static int asgn1_release(struct inode *inode, struct file *file)
{
	pr_info("asgn1: device closed\n");
	return 0;
}

/* ===== PHẦN 3: read / write ===== */

/*
 * asgn1_read - copy device_buffer ra userspace
 * Return: số byte đọc được, 0 = EOF, -EFAULT nếu địa chỉ user lỗi
 * Dùng copy_to_user() vì địa chỉ user có thể chưa được map hoặc không hợp lệ;
 * memcpy() sẽ gây kernel oops, còn copy_to_user() kiểm tra và bắt lỗi an toàn.
 */
static ssize_t asgn1_read(struct file *file, char __user *buf,
			  size_t len, loff_t *offset)
{
	ssize_t ret;

	mutex_lock(&asgn1_mutex);

	if (*offset >= buffer_len) {
		ret = 0;
		goto out;
	}
	if (len > buffer_len - *offset)
		len = buffer_len - *offset;

	if (copy_to_user(buf, device_buffer + *offset, len)) {
		ret = -EFAULT;
		goto out;
	}
	*offset += len;
	read_count++;
	ret = len;
	pr_info("asgn1: read %zu bytes (read_count=%d)\n", len, read_count);
out:
	mutex_unlock(&asgn1_mutex);
	return ret;
}

/*
 * asgn1_write - nhận dữ liệu từ userspace
 * Mode KERNEL: lưu "[KERNEL] " + dữ liệu. Mode ECHO: lưu nguyên bản.
 * Return: len gốc, hoặc -EFAULT
 */
static ssize_t asgn1_write(struct file *file, const char __user *buf,
			   size_t len, loff_t *offset)
{
	size_t pos = 0, max, n;

	mutex_lock(&asgn1_mutex);

	if (mode == ASGN1_MODE_KERNEL) {
		pos = strlen(KERNEL_PREFIX);
		memcpy(device_buffer, KERNEL_PREFIX, pos);
	}

	max = BUFFER_SIZE - 1 - pos;
	n = (len > max) ? max : len;
	if (n < len)
		pr_warn("asgn1: write too large, truncated to %zu bytes\n", n);

	if (copy_from_user(device_buffer + pos, buf, n)) {
		mutex_unlock(&asgn1_mutex);
		return -EFAULT;
	}

	buffer_len = pos + n;
	device_buffer[buffer_len] = '\0';
	write_count++;
	last_write_size = n;

	pr_info("asgn1: write %zu bytes, mode=%s (write_count=%d)\n",
		n, mode == ASGN1_MODE_KERNEL ? "KERNEL" : "ECHO", write_count);
	mutex_unlock(&asgn1_mutex);

	return len;
}

/* ===== PHẦN 4: ioctl ===== */

/*
 * asgn1_ioctl - xử lý 4 lệnh điều khiển
 * Return: 0 thành công, -ENOTTY lệnh sai, -EFAULT copy lỗi, -EINVAL mode sai
 */
static long asgn1_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	void __user *uarg = (void __user *)arg;
	struct asgn1_stats st;
	char ver[ASGN1_VERSION_LEN];
	int32_t new_mode;

	if (_IOC_TYPE(cmd) != ASGN1_MAGIC || _IOC_NR(cmd) > ASGN1_IOC_MAXNR)
		return -ENOTTY;

	switch (cmd) {
	case ASGN1_RESET_BUFFER:
		mutex_lock(&asgn1_mutex);
		memset(device_buffer, 0, BUFFER_SIZE);
		buffer_len = 0;
		write_count = 0;
		read_count = 0;
		last_write_size = 0;
		mutex_unlock(&asgn1_mutex);
		pr_info("asgn1: ioctl RESET_BUFFER - buffer and counters cleared\n");
		return 0;

	case ASGN1_GET_STATS:
		mutex_lock(&asgn1_mutex);
		st.open_count      = open_count;
		st.write_count     = write_count;
		st.read_count      = read_count;
		st.buffer_len      = buffer_len;
		st.last_write_size = last_write_size;
		st.mode            = mode;
		mutex_unlock(&asgn1_mutex);

		if (copy_to_user(uarg, &st, sizeof(st)))
			return -EFAULT;
		pr_info("asgn1: ioctl GET_STATS\n");
		return 0;

	case ASGN1_SET_MODE:
		if (copy_from_user(&new_mode, uarg, sizeof(new_mode)))
			return -EFAULT;
		if (new_mode != ASGN1_MODE_KERNEL && new_mode != ASGN1_MODE_ECHO)
			return -EINVAL;
		mutex_lock(&asgn1_mutex);
		mode = new_mode;
		mutex_unlock(&asgn1_mutex);
		pr_info("asgn1: ioctl SET_MODE -> %d (%s)\n", new_mode,
			new_mode == ASGN1_MODE_KERNEL ? "KERNEL" : "ECHO");
		return 0;

	case ASGN1_GET_VERSION:
		memset(ver, 0, sizeof(ver));
		strscpy(ver, DRIVER_VERSION, sizeof(ver));
		if (copy_to_user(uarg, ver, sizeof(ver)))
			return -EFAULT;
		pr_info("asgn1: ioctl GET_VERSION\n");
		return 0;

	default:
		return -ENOTTY;
	}
}

/* ===== PHẦN 5: Interface layer - đăng ký với kernel ===== */
static const struct file_operations asgn1_fops = {
	.owner          = THIS_MODULE,
	.open           = asgn1_open,
	.release        = asgn1_release,
	.read           = asgn1_read,
	.write          = asgn1_write,
	.unlocked_ioctl = asgn1_ioctl,
};

/* asgn1_init - insmod: đăng ký major, tạo class, device. Return: 0 hoặc lỗi âm */
static int __init asgn1_init(void)
{
	int ret;

	ret = register_chrdev(DRIVER_MAJOR, DEVICE_NAME, &asgn1_fops);
	if (ret < 0) {
		pr_err("asgn1: register_chrdev failed: %d\n", ret);
		return ret;
	}

	asgn1_class = class_create(THIS_MODULE, CLASS_NAME);
	if (IS_ERR(asgn1_class)) {
		ret = PTR_ERR(asgn1_class);
		goto err_chrdev;
	}

	asgn1_device = device_create(asgn1_class, NULL, MKDEV(DRIVER_MAJOR, 0),
				     NULL, DEVICE_NAME);
	if (IS_ERR(asgn1_device)) {
		ret = PTR_ERR(asgn1_device);
		goto err_class;
	}

	pr_info("asgn1: loaded, major=%d, ioctl magic='%c'\n",
		DRIVER_MAJOR, ASGN1_MAGIC);
	return 0;

err_class:
	class_destroy(asgn1_class);
err_chrdev:
	unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
	return ret;
}

/* asgn1_exit - rmmod: giải phóng ngược thứ tự init */
static void __exit asgn1_exit(void)
{
	device_destroy(asgn1_class, MKDEV(DRIVER_MAJOR, 0));
	class_destroy(asgn1_class);
	unregister_chrdev(DRIVER_MAJOR, DEVICE_NAME);
	pr_info("asgn1: module unloaded, all resources released\n");
}

module_init(asgn1_init);
module_exit(asgn1_exit);
