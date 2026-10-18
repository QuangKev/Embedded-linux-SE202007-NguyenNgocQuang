/*
 * counter_driver.c - Misc device mô phỏng bộ đếm phần cứng
 * Author : NguyenNgocQuang - SE202007V
 * Target : ARMv7 Cortex-A9 (QEMU vexpress-a9), Linux 5.15
 * Device : /dev/counter (major 10 = MISC_MAJOR, minor cấp động)
 * READ   : trả "<counter>\n" rồi tự tăng counter
 * WRITE  : ghi số nguyên >= 0 để set counter, sai định dạng -> -EINVAL
 * Proc   : /proc/counter_info
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NguyenNgocQuang - SE202007V");
MODULE_DESCRIPTION("ASSIGNMENT-01: misc counter device with /proc/counter_info");
MODULE_VERSION("1.0");

#define DEV_NAME    "counter"
#define PROC_NAME   "counter_info"
#define NUM_BUF_LEN 16

/* atomic_t: thao tác đọc-sửa-ghi không bị chen ngang (ARMv7 dùng LDREX/STREX).
 * Với int thường, counter++ gồm load-add-store, 2 CPU cùng làm có thể mất 1 lần tăng. */
static atomic_t counter      = ATOMIC_INIT(0);
static atomic_t total_reads  = ATOMIC_INIT(0);
static atomic_t total_sets   = ATOMIC_INIT(0);
static atomic_t total_resets = ATOMIC_INIT(0);

static struct proc_dir_entry *proc_entry;

/*
 * counter_read - trả giá trị counter hiện tại dạng chuỗi rồi tăng 1
 * @ppos: offset file. Lần đầu *ppos = 0 -> trả dữ liệu, tăng *ppos.
 *        Lần sau *ppos > 0 -> trả 0 (EOF) để cat dừng.
 * Return: số byte đã copy, 0 = EOF, -EFAULT/-EINVAL nếu lỗi
 */
static ssize_t counter_read(struct file *file, char __user *buf,
			    size_t len, loff_t *ppos)
{
	char kbuf[NUM_BUF_LEN];
	int val, n;

	if (*ppos > 0)
		return 0;

	/* lấy giá trị cũ và tăng trong 1 thao tác nguyên tử */
	val = atomic_fetch_inc(&counter);
	n = snprintf(kbuf, sizeof(kbuf), "%d\n", val);

	if (len < n)
		return -EINVAL;
	if (copy_to_user(buf, kbuf, n))
		return -EFAULT;

	*ppos += n;
	atomic_inc(&total_reads);
	pr_info("counter: read -> %d\n", val);
	return n;
}

/*
 * counter_write - parse số nguyên từ userspace và set counter
 * Return: len nếu OK; -EINVAL nếu không phải số hoặc số âm; -EFAULT nếu copy lỗi
 */
static ssize_t counter_write(struct file *file, const char __user *buf,
			     size_t len, loff_t *ppos)
{
	char kbuf[NUM_BUF_LEN];
	int val, ret;

	if (len == 0 || len >= sizeof(kbuf))
		return -EINVAL;
	if (copy_from_user(kbuf, buf, len))
		return -EFAULT;
	kbuf[len] = '\0';

	/* kstrtoint tự bỏ qua 1 ký tự '\n' ở cuối (do echo thêm vào) */
	ret = kstrtoint(kbuf, 10, &val);
	if (ret || val < 0) {
		pr_warn("counter: invalid input rejected\n");
		return -EINVAL;
	}

	atomic_set(&counter, val);
	atomic_inc(&total_sets);
	if (val == 0)
		atomic_inc(&total_resets);
	pr_info("counter: set to %d\n", val);
	return len;
}

static const struct file_operations counter_fops = {
	.owner  = THIS_MODULE,
	.read   = counter_read,
	.write  = counter_write,
	.llseek = no_llseek,
};

/* miscdevice: kernel tự dùng major 10, cấp minor động, tạo /dev/counter
 * qua devtmpfs. Không cần register_chrdev, class_create, device_create. */
static struct miscdevice counter_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = DEV_NAME,
	.fops  = &counter_fops,
	.mode  = 0666,
};

/* counter_proc_show - sinh nội dung /proc/counter_info (seq_file lo offset/buffer) */
static int counter_proc_show(struct seq_file *m, void *v)
{
	seq_puts(m,   "Counter Driver Statistics\n");
	seq_puts(m,   "=========================\n");
	seq_printf(m, "Device            : /dev/%s (major=10, minor=%d)\n",
		   DEV_NAME, counter_misc.minor);
	seq_printf(m, "Current value     : %d\n", atomic_read(&counter));
	seq_printf(m, "Total reads (inc) : %d\n", atomic_read(&total_reads));
	seq_printf(m, "Total writes (set): %d\n", atomic_read(&total_sets));
	seq_printf(m, "Total resets (=0) : %d\n", atomic_read(&total_resets));
	return 0;
}

static int counter_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, counter_proc_show, NULL);
}

static const struct proc_ops counter_proc_ops = {
	.proc_open    = counter_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/* counter_init - đăng ký misc device và tạo /proc entry. Return: 0 hoặc lỗi âm */
static int __init counter_init(void)
{
	int ret;

	ret = misc_register(&counter_misc);
	if (ret) {
		pr_err("counter: misc_register failed: %d\n", ret);
		return ret;
	}

	proc_entry = proc_create(PROC_NAME, 0444, NULL, &counter_proc_ops);
	if (!proc_entry) {
		misc_deregister(&counter_misc);
		return -ENOMEM;
	}

	pr_info("counter: registered /dev/%s major=10 minor=%d\n",
		DEV_NAME, counter_misc.minor);
	return 0;
}

/* counter_exit - gỡ /proc entry và misc device */
static void __exit counter_exit(void)
{
	proc_remove(proc_entry);
	misc_deregister(&counter_misc);
	pr_info("counter: module unloaded\n");
}

module_init(counter_init);
module_exit(counter_exit);
