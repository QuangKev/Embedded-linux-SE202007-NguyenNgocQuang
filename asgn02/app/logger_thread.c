/*
 * logger_thread.c
 * Purpose : Logger thread. Single writer of the log file, so the file needs
 *           no lock; other threads only push messages into the log queue.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "logger_thread.h"

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>

int logger_init(sms_ctx_t *ctx)
{
    struct stat st;

    ctx->log_fp = fopen(ctx->cfg.log_path, "a");
    if (!ctx->log_fp) {
        fprintf(stderr, "sms_app: cannot open log file %s: %s\n",
                ctx->cfg.log_path, strerror(errno));
        return -1;
    }
    ctx->log_bytes = (stat(ctx->cfg.log_path, &st) == 0) ? (long)st.st_size : 0L;
    return 0;
}

void logger_close(sms_ctx_t *ctx)
{
    if (ctx->log_fp) {
        fflush(ctx->log_fp);
        fclose(ctx->log_fp);
        ctx->log_fp = NULL;
    }
}

void logger_post(sms_ctx_t *ctx, const char *level, const char *fmt, ...)
{
    LogMsg  m;
    va_list ap;
    int     n;

    if (!ctx->log_buf)
        return;

    m.ts_ms = sms_realtime_ms();
    n = snprintf(m.text, sizeof(m.text), "[%s] ", level);
    if (n < 0 || (size_t)n >= sizeof(m.text))
        return;

    va_start(ap, fmt);
    vsnprintf(m.text + n, sizeof(m.text) - (size_t)n, fmt, ap);
    va_end(ap);

    cbuf_push(ctx->log_buf, &m);   /* full => dropped and counted in overflow_count */
}

/* Rename sms.log -> sms.log.1 and start a new file. */
static void rotate_log(sms_ctx_t *ctx)
{
    char rotated[256];

    fflush(ctx->log_fp);
    fclose(ctx->log_fp);
    ctx->log_fp = NULL;

    snprintf(rotated, sizeof(rotated), "%s.1", ctx->cfg.log_path);
    if (rename(ctx->cfg.log_path, rotated) != 0)
        fprintf(stderr, "sms_app: log rotate failed: %s\n", strerror(errno));

    ctx->log_fp = fopen(ctx->cfg.log_path, "w");
    ctx->log_bytes = 0;
    if (!ctx->log_fp) {
        fprintf(stderr, "sms_app: cannot reopen log file: %s\n", strerror(errno));
        atomic_store(&ctx->fatal_error, 1);
        atomic_store(&ctx->running, 0);
    }
}

static void write_line(sms_ctx_t *ctx, const LogMsg *m)
{
    struct tm tmv;
    char      stamp[32];
    time_t    secs = (time_t)(m->ts_ms / 1000LL);
    int       n;

    if (!ctx->log_fp)
        return;

    localtime_r(&secs, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    n = fprintf(ctx->log_fp, "%s.%03d %s\n", stamp, (int)(m->ts_ms % 1000LL), m->text);
    if (n > 0)
        ctx->log_bytes += n;

    if (ctx->log_bytes >= SMS_APP_LOG_MAX_BYTES)
        rotate_log(ctx);
}

void *logger_thread(void *arg)
{
    sms_ctx_t *ctx = arg;
    LogMsg     m;
    long long  last_flush = sms_monotonic_ms();
    int        rc;

    for (;;) {
        rc = cbuf_pop_timeout(ctx->log_buf, &m, 1000u);
        if (rc == CBUF_OK)
            write_line(ctx, &m);
        else if (rc == CBUF_CLOSED)
            break;                                   /* closed and drained */

        /* periodic flush instead of one flush per line */
        if (ctx->log_fp && sms_monotonic_ms() - last_flush >= (long long)SMS_APP_LOG_FLUSH_MS) {
            fflush(ctx->log_fp);
            last_flush = sms_monotonic_ms();
        }
    }

    if (ctx->log_fp)
        fflush(ctx->log_fp);                         /* final flush */
    return NULL;
}
