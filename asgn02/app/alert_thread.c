/*
 * alert_thread.c
 * Purpose : Alert thread. Sleeps in mq_receive() until the Processor sends an
 *           alarm, prints it, counts it and forwards it to the Logger.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "alert_thread.h"
#include "logger_thread.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

void *alert_thread(void *arg)
{
    sms_ctx_t *ctx = arg;
    AlarmMsg   m;
    ssize_t    n;

    for (;;) {
        n = mq_receive(ctx->mq_rx, (char *)&m, sizeof(m), NULL);   /* blocks here */
        if (n < 0) {
            if (errno == EINTR)
                continue;
            fprintf(stderr, "[SMS] mq_receive failed: %s\n", strerror(errno));
            logger_post(ctx, "ERROR", "mq_receive failed: %s", strerror(errno));
            atomic_store(&ctx->fatal_error, 1);
            break;
        }
        if ((size_t)n != sizeof(m))
            continue;                                              /* malformed, ignore */
        if (m.type == ALERT_STOP)
            break;                                                 /* graceful shutdown */
        if (m.type < 0 || m.type >= ALERT_TYPE_COUNT)
            continue;

        atomic_fetch_add(&ctx->stats.alerts[m.type], 1);

        fprintf(stderr, "[ALERT][%lld] TYPE: %s VALUE: %.1f THRESHOLD: %.1f\n",
                m.timestamp_ms, sms_alert_name(m.type), (double)m.value, (double)m.threshold);

        logger_post(ctx, "ALERT", "ts=%lld TYPE=%s VALUE=%.1f THRESHOLD=%.1f",
                    m.timestamp_ms, sms_alert_name(m.type), (double)m.value, (double)m.threshold);
    }
    return NULL;
}

int alert_send_stop(sms_ctx_t *ctx)
{
    AlarmMsg stop;
    int      i;

    memset(&stop, 0, sizeof(stop));
    stop.type = ALERT_STOP;

    /* The send side is non-blocking: if the queue is full, wait for the
     * Alert thread to drain it and retry (max ~1 s). */
    for (i = 0; i < 100; i++) {
        if (mq_send(ctx->mq_tx, (const char *)&stop, sizeof(stop), 31) == 0)
            return 0;
        if (errno != EAGAIN && errno != EINTR)
            break;
        usleep(10000);
    }
    return -1;
}
