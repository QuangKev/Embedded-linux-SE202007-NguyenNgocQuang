/*
 * sms_common.h
 * Purpose : Shared types, constants and the global context of the
 *           Sensor Monitoring System (SMS) userspace application.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef SMS_COMMON_H
#define SMS_COMMON_H

#include <mqueue.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#include "sms_sensor.h"      /* SMS_DEVICE_PATH, SMS_SET_INTERVAL (shared with the driver) */
#include "circular_buffer.h"

/* ------------------------------------------------------------------ */
/* Default configuration (can be overridden on the command line)       */
/* ------------------------------------------------------------------ */
#define SMS_APP_INTERVAL_MS       500u
#define SMS_APP_TEMP_THRESHOLD    40.0f
#define SMS_APP_HUM_THRESHOLD     80.0f
#define SMS_APP_DURATION_S        60u
#define SMS_APP_TEMP_SPIKE_DELTA  5.0f
#define SMS_APP_WINDOW_SIZE       10u          /* rolling average window */

#define SMS_APP_BUFFER_CAPACITY   64u          /* shared_buffer (sensor -> processor) */
#define SMS_APP_LOGQ_CAPACITY     256u         /* log queue (all -> logger)           */

#define SMS_APP_LOG_PATH          "/tmp/sms.log"
#ifndef SMS_APP_LOG_MAX_BYTES
#define SMS_APP_LOG_MAX_BYTES     (1024L * 1024L)   /* rotate above 1 MB */
#endif
#define SMS_APP_LOG_FLUSH_MS      5000u        /* periodic flush */
#define SMS_APP_LOG_TEXT_LEN      160

#define SMS_APP_JITTER_PATH       "/tmp/jitter_report.txt"
#define SMS_APP_JITTER_MAX        50000u       /* max jitter samples kept in memory */

#define SMS_APP_MQ_NAME           "/sms_alarm_queue"
#define SMS_APP_MQ_MAXMSG         8            /* <= /proc/sys/fs/mqueue/msg_max (default 10) */

#define SMS_APP_SIM_DEVICE        "sim"        /* "-D sim": built-in simulator (host testing) */
#define SMS_APP_RT_PRIO           50
#define SMS_APP_STACK_SIZE        (256u * 1024u)
#define SMS_APP_DRV_GUARD_DIV     10u          /* driver min interval = period - period/10 */
#define SMS_APP_EAGAIN_RETRIES    50
#define SMS_APP_MAX_CONSEC_FAIL   10u          /* consecutive read failures => fatal */

/* ------------------------------------------------------------------ */
/* Data types                                                          */
/* ------------------------------------------------------------------ */
typedef struct {
    long long timestamp_ms;      /* from the driver (ktime) */
    float     temperature;       /* degC */
    float     humidity;          /* %    */
} SensorData;

typedef enum {
    ALERT_TEMP_HIGH = 0,
    ALERT_HUMID_HIGH,
    ALERT_TEMP_SPIKE,
    ALERT_TYPE_COUNT,
    ALERT_STOP = 99              /* sentinel: wakes the Alert thread at shutdown */
} AlertType;

typedef struct {                 /* message carried by the POSIX mq */
    int       type;
    long long timestamp_ms;
    float     value;
    float     threshold;
} AlarmMsg;

typedef struct {                 /* message carried by the log queue */
    long long ts_ms;             /* wall-clock time of the event */
    char      text[SMS_APP_LOG_TEXT_LEN];
} LogMsg;

typedef struct {
    unsigned int interval_ms;
    float        temp_threshold;
    float        hum_threshold;
    unsigned int duration_s;     /* 0 = run until SIGINT */
    int          verbose;
    const char  *dev_path;
    const char  *log_path;
    const char  *jitter_path;
    int          rt_policy;      /* SCHED_FIFO or SCHED_OTHER (requested) */
    int          rt_prio;
} sms_config_t;

typedef struct {
    atomic_ulong samples_read;
    atomic_ulong read_errors;
    atomic_ulong samples_processed;
    atomic_ulong alerts[ALERT_TYPE_COUNT];   /* counted by the Alert thread */
    atomic_ulong alarms_dropped;             /* mq full */
    atomic_ulong overruns;                   /* sensor period overrun */
} sms_stats_t;

typedef struct {
    sms_config_t   cfg;
    sms_stats_t    stats;

    atomic_int     running;          /* 1 while the system is running           */
    atomic_int     fatal_error;      /* set when a thread hits an unrecoverable error */

    int            dev_fd;           /* /dev/sms_sensor (or -1 in sim mode)     */
    cbuf_t        *sensor_buf;       /* shared_buffer: Sensor -> Processor      */
    cbuf_t        *log_buf;          /* log_queue:     all    -> Logger         */
    mqd_t          mq_tx;            /* alarm_queue, send side (non-blocking)   */
    mqd_t          mq_rx;            /* alarm_queue, receive side (blocking)    */

    FILE          *log_fp;           /* owned by the Logger thread while running */
    long           log_bytes;

    struct timespec start_ts;        /* CLOCK_MONOTONIC at start */

    int32_t       *jitter_us;        /* written only by the Sensor thread */
    size_t         jitter_count;
    int            rt_policy_active; /* policy really in effect for the Sensor thread */
    int            rt_prio_active;
} sms_ctx_t;

/* ------------------------------------------------------------------ */
/* Helpers (sms_common.c)                                              */
/* ------------------------------------------------------------------ */
const char *sms_alert_name(int type);
const char *sms_policy_name(int policy);
long long   sms_realtime_ms(void);
long long   sms_monotonic_ms(void);
void        sms_timespec_add_ms(struct timespec *ts, unsigned long ms);
long long   sms_timespec_diff_us(const struct timespec *a, const struct timespec *b); /* a - b */

#endif /* SMS_COMMON_H */
