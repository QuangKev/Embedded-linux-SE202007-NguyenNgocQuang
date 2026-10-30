/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LAB2_IOCTL_H
#define LAB2_IOCTL_H
#include <linux/ioctl.h>                                                                                // Dung chung giua module va ung dung userspace
#include <linux/types.h>                                                                                // __u32 luon la so nguyen 32-bit
#define LAB2_GET_BUFFER_LEN _IOR('L', 1, __u32)                                                         // Kernel tra do dai qua con tro __u32
#define LAB2_CLEAR_BUFFER   _IO('L', 2)                                                                 // Khong truyen du lieu
#endif
