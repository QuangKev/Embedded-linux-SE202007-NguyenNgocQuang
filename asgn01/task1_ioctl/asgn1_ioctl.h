/*
 * asgn1_ioctl.h - Định nghĩa ioctl dùng CHUNG cho kernel module và userspace
 * Author : NguyenNgocQuang - <SE202007V>
 */
#ifndef ASGN1_IOCTL_H
#define ASGN1_IOCTL_H

#ifdef __KERNEL__
#include <linux/ioctl.h>
#include <linux/types.h>
#else
#include <sys/ioctl.h>
#include <stdint.h>
#endif

#define ASGN1_MAGIC         'q'
#define ASGN1_VERSION_LEN   64

#define ASGN1_MODE_KERNEL   0
#define ASGN1_MODE_ECHO     1

struct asgn1_stats {
	int32_t open_count;
	int32_t write_count;
	int32_t read_count;
	int32_t buffer_len;
	int32_t last_write_size;
	int32_t mode;
};

#define ASGN1_RESET_BUFFER  _IO(ASGN1_MAGIC, 0)
#define ASGN1_GET_STATS     _IOR(ASGN1_MAGIC, 1, struct asgn1_stats)
#define ASGN1_SET_MODE      _IOW(ASGN1_MAGIC, 2, int32_t)
#define ASGN1_GET_VERSION   _IOR(ASGN1_MAGIC, 3, char[ASGN1_VERSION_LEN])

#define ASGN1_IOC_MAXNR     3

#endif
