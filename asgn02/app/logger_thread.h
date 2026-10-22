/*
 * logger_thread.h
 * Purpose : Logger thread API: receives LogMsg from the log queue and writes
 *           /tmp/sms.log (rotate at 1 MB, periodic flush every 5 s).
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef LOGGER_THREAD_H
#define LOGGER_THREAD_H

#include "sms_common.h"

/* Opens the log file (append). Returns 0 on success, -1 on error (message printed). */
int logger_init(sms_ctx_t *ctx);

/* Closes the log file opened by logger_init (call after the thread was joined). */
void logger_close(sms_ctx_t *ctx);

/* Thread-safe, non-blocking: formats "[LEVEL] message" and queues it for the Logger. */
void logger_post(sms_ctx_t *ctx, const char *level, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

void *logger_thread(void *arg);

#endif /* LOGGER_THREAD_H */
