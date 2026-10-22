/*
 * sensor_thread.h
 * Purpose : Sensor thread API: periodic read of /dev/sms_sensor (SCHED_FIFO,
 *           clock_nanosleep with absolute time) + jitter measurement.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef SENSOR_THREAD_H
#define SENSOR_THREAD_H

#include "sms_common.h"

void *sensor_thread(void *arg);

/* Appends Min/Avg/Max/P99 + text histogram of the collected jitter to
 * cfg.jitter_path. Call after the Sensor thread was joined. Returns 0 on success. */
int sensor_write_jitter_report(const sms_ctx_t *ctx);

#endif /* SENSOR_THREAD_H */
