/*
 * sms_sensor_driver.c
 * Purpose : Simulated temperature/humidity sensor.
 *           - /dev/sms_sensor : each read() returns "<timestamp_ms>,<temp>,<hum>\n"
 *           - /proc/sms_stats : read_count, last values, uptime
 *           - ioctl SMS_SET_INTERVAL : min interval between two reads (ms)
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-20
 *
 * Note: kernel has no floating point -> values are stored as integer x10
 *       (253 means 25.3 C) and printed as "25.3".
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/random.h>
#include <linux/ktime.h>
#include <linux/delay.h>
#include <linux/math64.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include "sms_sensor.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Nguyen Ngoc Quang - SE202007");
MODULE_DESCRIPTION("SMS simulated temperature/humidity sensor");

/* Ranges (x10) */
#define TEMP_MIN_X10   150u   /* 15.0 C */
#define TEMP_MAX_X10   450u   /* 45.0 C */
#define HUM_MIN_X10    300u   /* 30.0 % */
#define HUM_MAX_X10    900u   /* 90.0 % */

struct sms_state {
	struct mutex  lock;
	unsigned long read_count;
	unsigned int  last_temp_x10;
	unsigned int  last_hum_x10;
	unsigned int  interval_ms;
	u64           last_read_ms;
	u64           start_ms;
};

static struct sms_state sms = {
	.interval_ms = SMS_DEFAULT_INTERVAL_MS,
};

static inline u64 sms_now_ms(void)
{
	return ktime_to_ms(ktime_get());
}

static unsigned int rand_range(unsigned int min, unsigned int max)
{
	return min + (get_random_u32() % (max - min + 1));
}

/* ---------------- file_operations ---------------- */

static ssize_t sms_read(struct file *f, char __user *buf, size_t count, loff_t *ppos)
{
	char line[64];
	int len;
	u64 now, elapsed;
	unsigned int temp, hum;

	/* Wait until interval has elapsed (or -EAGAIN if O_NONBLOCK) */
	for (;;) {
		if (mutex_lock_interruptible(&sms.lock))
			return -ERESTARTSYS;

		now = sms_now_ms();
		elapsed = now - sms.last_read_ms;
		if (sms.read_count == 0 || elapsed >= sms.interval_ms)
			break;                       /* keep lock, go generate data */

		{
			unsigned int wait_ms = sms.interval_ms - (unsigned int)elapsed;

			mutex_unlock(&sms.lock);
			if (f->f_flags & O_NONBLOCK)
				return -EAGAIN;
			if (msleep_interruptible(wait_ms))
				return -ERESTARTSYS;
		}
	}

	temp = rand_range(TEMP_MIN_X10, TEMP_MAX_X10);
	hum  = rand_range(HUM_MIN_X10, HUM_MAX_X10);

	sms.last_temp_x10 = temp;
	sms.last_hum_x10  = hum;
	sms.last_read_ms  = now;
	sms.read_count++;

	mutex_unlock(&sms.lock);

	len = scnprintf(line, sizeof(line), "%llu,%u.%u,%u.%u\n",
			now, temp / 10, temp % 10, hum / 10, hum % 10);

	if (count < (size_t)len)
		return -EINVAL;
	if (copy_to_user(buf, line, len))
		return -EFAULT;

	return len;
}

static long sms_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	unsigned int val;

	switch (cmd) {
	case SMS_SET_INTERVAL:
		if (get_user(val, (unsigned int __user *)arg))
			return -EFAULT;
		if (val < SMS_MIN_INTERVAL_MS || val > SMS_MAX_INTERVAL_MS)
			return -EINVAL;
		if (mutex_lock_interruptible(&sms.lock))
			return -ERESTARTSYS;
		sms.interval_ms = val;
		mutex_unlock(&sms.lock);
		pr_info("sms_sensor: interval set to %u ms\n", val);
		return 0;
	default:
		return -ENOTTY;
	}
}

static const struct file_operations sms_fops = {
	.owner          = THIS_MODULE,
	.read           = sms_read,
	.unlocked_ioctl = sms_ioctl,
	.llseek         = no_llseek,
};

static struct miscdevice sms_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name  = SMS_DEVICE_NAME,
	.fops  = &sms_fops,
	.mode  = 0666,
};

/* ---------------- /proc/sms_stats ---------------- */

static int sms_proc_show(struct seq_file *m, void *v)
{
	mutex_lock(&sms.lock);
	seq_printf(m, "read_count: %lu\n", sms.read_count);
	seq_printf(m, "last_temperature: %u.%u\n",
		   sms.last_temp_x10 / 10, sms.last_temp_x10 % 10);
	seq_printf(m, "last_humidity: %u.%u\n",
		   sms.last_hum_x10 / 10, sms.last_hum_x10 % 10);
	seq_printf(m, "uptime_sec: %llu\n", div_u64(sms_now_ms() - sms.start_ms, 1000));
	seq_printf(m, "interval_ms: %u\n", sms.interval_ms);
	mutex_unlock(&sms.lock);
	return 0;
}

static int sms_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, sms_proc_show, NULL);
}

static const struct proc_ops sms_proc_ops = {
	.proc_open    = sms_proc_open,
	.proc_read    = seq_read,
	.proc_lseek   = seq_lseek,
	.proc_release = single_release,
};

/* ---------------- init / exit ---------------- */

static int __init sms_init(void)
{
	int ret;

	mutex_init(&sms.lock);
	sms.start_ms = sms_now_ms();

	ret = misc_register(&sms_misc);
	if (ret) {
		pr_err("sms_sensor: misc_register failed (%d)\n", ret);
		return ret;
	}

	if (!proc_create(SMS_PROC_NAME, 0444, NULL, &sms_proc_ops)) {
		misc_deregister(&sms_misc);
		pr_err("sms_sensor: proc_create failed\n");
		return -ENOMEM;
	}

	pr_info("sms_sensor: loaded, /dev/%s ready\n", SMS_DEVICE_NAME);
	return 0;
}

static void __exit sms_exit(void)
{
	remove_proc_entry(SMS_PROC_NAME, NULL);
	misc_deregister(&sms_misc);
	pr_info("sms_sensor: unloaded\n");
}

module_init(sms_init);
module_exit(sms_exit);
