/*
 * sms_common.c
 * Purpose : Small helpers shared by all threads (names, clocks, timespec math).
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "sms_common.h"

const char *sms_alert_name(int type)
{
    switch (type) {
    case ALERT_TEMP_HIGH:  return "TEMP_HIGH";
    case ALERT_HUMID_HIGH: return "HUMID_HIGH";
    case ALERT_TEMP_SPIKE: return "TEMP_SPIKE";
    default:               return "UNKNOWN";
    }
}

const char *sms_policy_name(int policy)
{
    switch (policy) {
    case SCHED_FIFO: return "SCHED_FIFO";
    case SCHED_RR:   return "SCHED_RR";
    case SCHED_OTHER: return "SCHED_OTHER";
    default:         return "SCHED_?";
    }
}

long long sms_realtime_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L;
}

long long sms_monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000L;
}

void sms_timespec_add_ms(struct timespec *ts, unsigned long ms)
{
    ts->tv_sec  += (time_t)(ms / 1000UL);
    ts->tv_nsec += (long)(ms % 1000UL) * 1000000L;
    if (ts->tv_nsec >= 1000000000L) {
        ts->tv_sec  += 1;
        ts->tv_nsec -= 1000000000L;
    }
}

long long sms_timespec_diff_us(const struct timespec *a, const struct timespec *b)
{
    return ((long long)a->tv_sec - (long long)b->tv_sec) * 1000000LL +
           ((long long)a->tv_nsec - (long long)b->tv_nsec) / 1000LL;
}
