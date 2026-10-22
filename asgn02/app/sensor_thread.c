/*
 * sensor_thread.c
 * Purpose : Sensor thread (producer).
 *           - optional SCHED_FIFO real-time scheduling
 *           - fixed period with clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME)
 *           - measures wake-up jitter (actual - expected) for every period
 *           - reads one CSV line "<ts_ms>,<temp>,<hum>\n" from the driver,
 *             parses it and pushes a SensorData into shared_buffer
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "sensor_thread.h"
#include "logger_thread.h"

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINE_LEN 96

/* ------------------------------------------------------------------ */
/* Built-in simulator, used only with "-D sim" (host build for Valgrind */
/* / perf where /dev/sms_sensor does not exist). Same CSV format.       */
/* ------------------------------------------------------------------ */
static ssize_t sim_read(char *buf, size_t len)
{
    static unsigned int seed = 0;              /* only touched by the Sensor thread */
    unsigned int t, h;
    int n;

    if (seed == 0)
        seed = (unsigned int)sms_monotonic_ms() | 1u;
    t = 150u + (unsigned int)rand_r(&seed) % 301u;     /* 15.0 .. 45.0 */
    h = 300u + (unsigned int)rand_r(&seed) % 601u;     /* 30.0 .. 90.0 */
    n = snprintf(buf, len, "%lld,%u.%u,%u.%u\n",
                 sms_monotonic_ms(), t / 10u, t % 10u, h / 10u, h % 10u);
    return (n > 0) ? (ssize_t)n : -1;
}

static int parse_line(const char *line, SensorData *out)
{
    long long ts;
    float     t, h;

    if (sscanf(line, "%lld,%f,%f", &ts, &t, &h) != 3)
        return -1;
    out->timestamp_ms = ts;
    out->temperature  = t;
    out->humidity     = h;
    return 0;
}

/* Returns 0 = sample read, 1 = interrupted by shutdown, -1 = error (already logged). */
static int read_sample(sms_ctx_t *ctx, SensorData *sd)
{
    char    line[LINE_LEN];
    ssize_t n;
    int     eagain_tries = 0;

    for (;;) {
        if (ctx->dev_fd < 0)
            n = sim_read(line, sizeof(line) - 1);
        else
            n = read(ctx->dev_fd, line, sizeof(line) - 1);

        if (n > 0) {
            line[n] = '\0';
            if (parse_line(line, sd) == 0)
                return 0;
            logger_post(ctx, "WARN", "cannot parse sensor line: %.40s", line);
            return -1;
        }
        if (n == 0) {                                       /* EOF */
            logger_post(ctx, "WARN", "EOF while reading sensor device");
            return -1;
        }
        if (errno == EINTR) {                               /* signal: retry unless stopping */
            if (!atomic_load(&ctx->running))
                return 1;
            continue;
        }
        if (errno == EAGAIN) {                              /* driver not ready: short retry */
            if (++eagain_tries > SMS_APP_EAGAIN_RETRIES) {
                logger_post(ctx, "WARN", "sensor device busy (EAGAIN), sample skipped");
                return -1;
            }
            usleep(1000);
            continue;
        }
        logger_post(ctx, "ERROR", "read(%s) failed: %s", ctx->cfg.dev_path, strerror(errno));
        return -1;
    }
}

/* ------------------------------------------------------------------ */
/* Real-time setup                                                      */
/* ------------------------------------------------------------------ */
static void setup_rt(sms_ctx_t *ctx)
{
    struct sched_param sp;
    int policy = SCHED_OTHER;
    int rc;

    if (ctx->cfg.rt_policy == SCHED_FIFO) {
        memset(&sp, 0, sizeof(sp));
        sp.sched_priority = ctx->cfg.rt_prio;
        rc = pthread_setschedparam(pthread_self(), SCHED_FIFO, &sp);
        if (rc != 0) {
            fprintf(stderr, "[SMS] WARNING: SCHED_FIFO prio %d failed (%s) -> running as SCHED_OTHER\n",
                    ctx->cfg.rt_prio, strerror(rc));
            logger_post(ctx, "WARN", "SCHED_FIFO prio %d failed: %s", ctx->cfg.rt_prio, strerror(rc));
        }
    }

    memset(&sp, 0, sizeof(sp));
    pthread_getschedparam(pthread_self(), &policy, &sp);
    ctx->rt_policy_active = policy;
    ctx->rt_prio_active   = sp.sched_priority;
    logger_post(ctx, "INFO", "sensor thread scheduling: %s prio=%d",
                sms_policy_name(policy), sp.sched_priority);
}

/* ------------------------------------------------------------------ */
/* Thread body                                                          */
/* ------------------------------------------------------------------ */
void *sensor_thread(void *arg)
{
    sms_ctx_t       *ctx = arg;
    struct timespec  next, actual;
    unsigned int     consecutive_fail = 0;
    long long        period_us = (long long)ctx->cfg.interval_ms * 1000LL;
    sigset_t         usr1;

    /* SIGUSR1 is blocked in every thread except this one; main() uses it to
     * interrupt clock_nanosleep()/read() at shutdown. */
    sigemptyset(&usr1);
    sigaddset(&usr1, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &usr1, NULL);

    setup_rt(ctx);

    clock_gettime(CLOCK_MONOTONIC, &next);

    while (atomic_load(&ctx->running)) {
        SensorData sd;
        long long  jitter_us;
        int        rc, r;

        /* Absolute deadline => no drift, unlike usleep(). */
        sms_timespec_add_ms(&next, ctx->cfg.interval_ms);
        do {
            rc = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &next, NULL);
        } while (rc == EINTR && atomic_load(&ctx->running));
        if (!atomic_load(&ctx->running))
            break;

        /* Jitter = actual wake-up time - expected wake-up time. */
        clock_gettime(CLOCK_MONOTONIC, &actual);
        jitter_us = sms_timespec_diff_us(&actual, &next);
        if (ctx->jitter_count < SMS_APP_JITTER_MAX)
            ctx->jitter_us[ctx->jitter_count++] = (int32_t)jitter_us;

        if (jitter_us > period_us) {              /* missed a whole period: resynchronise */
            atomic_fetch_add(&ctx->stats.overruns, 1);
            next = actual;
        }

        r = read_sample(ctx, &sd);
        if (r == 1)
            break;
        if (r < 0) {
            atomic_fetch_add(&ctx->stats.read_errors, 1);
            if (++consecutive_fail >= SMS_APP_MAX_CONSEC_FAIL) {
                fprintf(stderr, "[SMS] ERROR: %u consecutive sensor read failures, stopping\n",
                        consecutive_fail);
                logger_post(ctx, "ERROR", "%u consecutive read failures, stopping", consecutive_fail);
                atomic_store(&ctx->fatal_error, 1);
                atomic_store(&ctx->running, 0);
                break;
            }
            continue;
        }
        consecutive_fail = 0;
        atomic_fetch_add(&ctx->stats.samples_read, 1);

        if (cbuf_push(ctx->sensor_buf, &sd) == CBUF_FULL)
            logger_post(ctx, "WARN", "shared_buffer full, sample ts=%lld dropped", sd.timestamp_ms);
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Jitter report                                                        */
/* ------------------------------------------------------------------ */
static int cmp_i32(const void *a, const void *b)
{
    int32_t x = *(const int32_t *)a;
    int32_t y = *(const int32_t *)b;

    return (x > y) - (x < y);
}

int sensor_write_jitter_report(const sms_ctx_t *ctx)
{
    size_t   n = ctx->jitter_count, i, p99_idx;
    int32_t *sorted;
    double   sum = 0.0;
    size_t   bucket[4] = { 0, 0, 0, 0 };
    FILE    *fp;
    time_t   now = time(NULL);
    struct tm tmv;
    char     stamp[32];
    static const char *label[4] = { "0-100 us", "100-500 us", "500-1000 us", ">1 ms" };

    fp = fopen(ctx->cfg.jitter_path, "a");
    if (!fp) {
        fprintf(stderr, "sms_app: cannot open jitter report %s: %s\n",
                ctx->cfg.jitter_path, strerror(errno));
        return -1;
    }

    localtime_r(&now, &tmv);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tmv);

    fprintf(fp, "=== SMS jitter report (%s) ===\n", stamp);
    fprintf(fp, "Requested policy : %s (prio %d)\n",
            sms_policy_name(ctx->cfg.rt_policy), ctx->cfg.rt_prio);
    fprintf(fp, "Active policy    : %s (prio %d)\n",
            sms_policy_name(ctx->rt_policy_active), ctx->rt_prio_active);
    fprintf(fp, "Period           : %u ms\n", ctx->cfg.interval_ms);
    fprintf(fp, "Samples          : %zu\n", n);

    if (n == 0) {
        fprintf(fp, "(no jitter samples collected)\n\n");
        fclose(fp);
        return 0;
    }

    sorted = malloc(n * sizeof(*sorted));
    if (!sorted) {
        fclose(fp);
        return -1;
    }
    memcpy(sorted, ctx->jitter_us, n * sizeof(*sorted));
    qsort(sorted, n, sizeof(*sorted), cmp_i32);

    for (i = 0; i < n; i++) {
        int32_t v = ctx->jitter_us[i];

        sum += (double)v;
        if (v < 100)       bucket[0]++;
        else if (v < 500)  bucket[1]++;
        else if (v < 1000) bucket[2]++;
        else               bucket[3]++;
    }

    p99_idx = (n * 99u + 99u) / 100u;            /* nearest-rank: ceil(0.99 * n) */
    if (p99_idx == 0)
        p99_idx = 1;

    fprintf(fp, "Min / Avg / Max  : %d / %.1f / %d us\n",
            (int)sorted[0], sum / (double)n, (int)sorted[n - 1]);
    fprintf(fp, "P99              : %d us\n", (int)sorted[p99_idx - 1]);
    fprintf(fp, "Histogram:\n");
    for (i = 0; i < 4; i++)
        fprintf(fp, "  %-12s : %6zu (%5.1f%%)\n", label[i], bucket[i],
                100.0 * (double)bucket[i] / (double)n);
    fprintf(fp, "\n");

    free(sorted);
    fclose(fp);
    return 0;
}
