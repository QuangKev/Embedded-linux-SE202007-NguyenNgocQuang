/*
 * sms_sensor.h
 * Purpose : Shared definitions between sms_sensor driver (kernel) and sms_app (userspace)
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-20
 */
#ifndef SMS_SENSOR_H
#define SMS_SENSOR_H

#include <linux/ioctl.h>

#define SMS_DEVICE_NAME          "sms_sensor"
#define SMS_DEVICE_PATH          "/dev/sms_sensor"
#define SMS_PROC_NAME            "sms_stats"

#define SMS_DEFAULT_INTERVAL_MS  100u
#define SMS_MIN_INTERVAL_MS      1u
#define SMS_MAX_INTERVAL_MS      60000u

/* ioctl: set minimum interval (ms) between two reads */
#define SMS_IOC_MAGIC            'S'
#define SMS_SET_INTERVAL         _IOW(SMS_IOC_MAGIC, 1, unsigned int)

#endif /* SMS_SENSOR_H */
