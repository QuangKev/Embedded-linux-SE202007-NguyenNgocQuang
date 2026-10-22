/*
 * alert_thread.h
 * Purpose : Alert thread API: blocks on the POSIX message queue (alarm_queue),
 *           prints alerts to stderr, counts them per type and logs them.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef ALERT_THREAD_H
#define ALERT_THREAD_H

#include "sms_common.h"

void *alert_thread(void *arg);

/* Used by shutdown code: sends the ALERT_STOP sentinel so the thread leaves mq_receive(). */
int alert_send_stop(sms_ctx_t *ctx);

#endif /* ALERT_THREAD_H */
