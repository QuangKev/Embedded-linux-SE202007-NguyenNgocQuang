/*
 * sms_app.c
 * Purpose : Sensor Monitoring System - main program.
 *           Parses arguments, creates the resources (device, buffers, POSIX mq,
 *           log file), starts the 4 threads, waits for the duration / SIGINT
 *           and performs a graceful shutdown.
 *
 *           State machine:  INIT -> RUNNING -> SHUTDOWN
 *             INIT     : parse args, open device, create buffers/mq, start threads
 *             RUNNING  : 4 threads working; main waits (sigtimedwait)
 *             SHUTDOWN : triggered by SIGINT/SIGTERM, duration elapsed or fatal error
 *
 *           Lock order: no thread ever holds two locks at once (each cbuf has
 *           its own mutex, taken alone), therefore no deadlock cycle exists.
 *
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include "alert_thread.h"
#include "logger_thread.h"
#include "processor_thread.h"
#include "sensor_thread.h"
#include "sms_common.h"

#define EXIT_RUNTIME 1
#define EXIT_USAGE   2

typedef enum { STATE_INIT, STATE_RUNNING, STATE_SHUTDOWN } app_state_t;

static const char *state_name(app_state_t s)
{
    return (s == STATE_INIT) ? "INIT" : (s == STATE_RUNNING) ? "RUNNING" : "SHUTDOWN";
}

static void change_state(sms_ctx_t *ctx, app_state_t *cur, app_state_t next)
{
    logger_post(ctx, "STATE", "%s -> %s", state_name(*cur), state_name(next));
    *cur = next;
}

static void sigusr1_handler(int sig)
{
    (void)sig;                      /* only purpose: make blocking calls return EINTR */
}

/* ------------------------------------------------------------------ */
/* Argument parsing                                                     */
/* ------------------------------------------------------------------ */
static void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [OPTIONS]\n"
        "  -i <ms>    Sensor sampling interval (default: %u ms)\n"
        "  -t <C>     Temperature alert threshold (default: %.1f)\n"
        "  -h <%%>     Humidity alert threshold (default: %.1f)\n"
        "  -d <sec>   Run duration in seconds, 0 = infinite (default: %u)\n"
        "  -v         Verbose mode: print each sample to stdout\n"
        "Extra options:\n"
        "  -D <path>  Sensor device (default: %s; \"%s\" = built-in simulator for host tests)\n"
        "  -l <file>  Log file (default: %s)\n"
        "  -j <file>  Jitter report file, appended (default: %s)\n"
        "  -S <pol>   Sensor thread policy: fifo | other (default: fifo)\n"
        "  -p <prio>  SCHED_FIFO priority 1..99 (default: %d)\n",
        prog, SMS_APP_INTERVAL_MS, (double)SMS_APP_TEMP_THRESHOLD, (double)SMS_APP_HUM_THRESHOLD,
        SMS_APP_DURATION_S, SMS_DEVICE_PATH, SMS_APP_SIM_DEVICE, SMS_APP_LOG_PATH,
        SMS_APP_JITTER_PATH, SMS_APP_RT_PRIO);
}

static int parse_uint(const char *s, unsigned long *out)
{
    char *end;

    if (!s || *s == '\0' || *s == '-')
        return -1;
    errno = 0;
    *out = strtoul(s, &end, 10);
    return (errno != 0 || *end != '\0') ? -1 : 0;
}

static int parse_float(const char *s, float *out)
{
    char *end;

    if (!s || *s == '\0')
        return -1;
    errno = 0;
    *out = strtof(s, &end);
    return (errno != 0 || *end != '\0') ? -1 : 0;
}

/* Returns 0 on success, -1 on invalid arguments (message already printed). */
static int parse_args(int argc, char **argv, sms_config_t *cfg)
{
    int           opt;
    unsigned long v;

    cfg->interval_ms    = SMS_APP_INTERVAL_MS;
    cfg->temp_threshold = SMS_APP_TEMP_THRESHOLD;
    cfg->hum_threshold  = SMS_APP_HUM_THRESHOLD;
    cfg->duration_s     = SMS_APP_DURATION_S;
    cfg->verbose        = 0;
    cfg->dev_path       = SMS_DEVICE_PATH;
    cfg->log_path       = SMS_APP_LOG_PATH;
    cfg->jitter_path    = SMS_APP_JITTER_PATH;
    cfg->rt_policy      = SCHED_FIFO;
    cfg->rt_prio        = SMS_APP_RT_PRIO;

    while ((opt = getopt(argc, argv, "i:t:h:d:vD:l:j:S:p:")) != -1) {
        switch (opt) {
        case 'i':
            if (parse_uint(optarg, &v) || v < SMS_MIN_INTERVAL_MS || v > SMS_MAX_INTERVAL_MS) {
                fprintf(stderr, "sms_app: invalid -i value '%s' (%u..%u ms)\n",
                        optarg, SMS_MIN_INTERVAL_MS, SMS_MAX_INTERVAL_MS);
                return -1;
            }
            cfg->interval_ms = (unsigned int)v;
            break;
        case 't':
            if (parse_float(optarg, &cfg->temp_threshold)) {
                fprintf(stderr, "sms_app: invalid -t value '%s'\n", optarg);
                return -1;
            }
            break;
        case 'h':
            if (parse_float(optarg, &cfg->hum_threshold)) {
                fprintf(stderr, "sms_app: invalid -h value '%s'\n", optarg);
                return -1;
            }
            break;
        case 'd':
            if (parse_uint(optarg, &v) || v > 86400UL) {
                fprintf(stderr, "sms_app: invalid -d value '%s' (0..86400 s)\n", optarg);
                return -1;
            }
            cfg->duration_s = (unsigned int)v;
            break;
        case 'v': cfg->verbose = 1;      break;
        case 'D': cfg->dev_path = optarg;    break;
        case 'l': cfg->log_path = optarg;    break;
        case 'j': cfg->jitter_path = optarg; break;
        case 'S':
            if (strcmp(optarg, "fifo") == 0)
                cfg->rt_policy = SCHED_FIFO;
            else if (strcmp(optarg, "other") == 0)
                cfg->rt_policy = SCHED_OTHER;
            else {
                fprintf(stderr, "sms_app: invalid -S value '%s' (fifo|other)\n", optarg);
                return -1;
            }
            break;
        case 'p':
            if (parse_uint(optarg, &v) || v < 1 || v > 99) {
                fprintf(stderr, "sms_app: invalid -p value '%s' (1..99)\n", optarg);
                return -1;
            }
            cfg->rt_prio = (int)v;
            break;
        default:
            return -1;
        }
    }
    if (optind < argc) {
        fprintf(stderr, "sms_app: unexpected argument '%s'\n", argv[optind]);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Resources                                                            */
/* ------------------------------------------------------------------ */
static int init_resources(sms_ctx_t *ctx)
{
    struct mq_attr attr;

    if (logger_init(ctx) != 0)
        return -1;

    /* Sensor device. Not hard-coded: comes from -D / SMS_DEVICE_PATH. */
    if (strcmp(ctx->cfg.dev_path, SMS_APP_SIM_DEVICE) == 0) {
        ctx->dev_fd = -1;                                   /* simulator mode */
        fprintf(stderr, "[SMS] NOTE: using built-in sensor simulator (no driver)\n");
    } else {
        unsigned int interval = ctx->cfg.interval_ms;

        /* The driver enforces "minimum interval since the last read" and sleeps when a
         * read comes too early. Measured on QEMU: with min interval == period that sleep
         * accumulated into a saw-tooth delay (jitter 0..22 ms, ~8% of the periods lost).
         * The application's clock_nanosleep() already paces the reads, so the driver is
         * only a guard: use 90% of the period (min 1 ms) so it never blocks a read. */
        if (interval > 1u) {
            unsigned int margin = interval / SMS_APP_DRV_GUARD_DIV;

            interval -= (margin > 0u) ? margin : 1u;
        }

        ctx->dev_fd = open(ctx->cfg.dev_path, O_RDONLY);
        if (ctx->dev_fd < 0) {
            fprintf(stderr, "sms_app: cannot open %s: %s (is sms_sensor_driver.ko loaded?)\n",
                    ctx->cfg.dev_path, strerror(errno));
            return -1;
        }
        /* make the driver's minimum read interval match the application's period */
        if (ioctl(ctx->dev_fd, SMS_SET_INTERVAL, &interval) != 0) {
            fprintf(stderr, "sms_app: ioctl(SMS_SET_INTERVAL, %u) failed: %s\n",
                    interval, strerror(errno));
            return -1;
        }
    }

    ctx->sensor_buf = cbuf_create(SMS_APP_BUFFER_CAPACITY, sizeof(SensorData));
    ctx->log_buf    = cbuf_create(SMS_APP_LOGQ_CAPACITY, sizeof(LogMsg));
    if (!ctx->sensor_buf || !ctx->log_buf) {
        fprintf(stderr, "sms_app: out of memory (buffers)\n");
        return -1;
    }

    ctx->jitter_us = calloc(SMS_APP_JITTER_MAX, sizeof(*ctx->jitter_us));
    if (!ctx->jitter_us) {
        fprintf(stderr, "sms_app: out of memory (jitter array)\n");
        return -1;
    }

    /* POSIX message queue Processor -> Alert */
    memset(&attr, 0, sizeof(attr));
    attr.mq_maxmsg  = SMS_APP_MQ_MAXMSG;
    attr.mq_msgsize = (long)sizeof(AlarmMsg);
    mq_unlink(SMS_APP_MQ_NAME);                             /* drop a stale queue, if any */
    ctx->mq_rx = mq_open(SMS_APP_MQ_NAME, O_RDONLY | O_CREAT | O_EXCL, 0600, &attr);
    if (ctx->mq_rx == (mqd_t)-1) {
        fprintf(stderr, "sms_app: mq_open(%s) failed: %s (kernel needs CONFIG_POSIX_MQUEUE)\n",
                SMS_APP_MQ_NAME, strerror(errno));
        return -1;
    }
    ctx->mq_tx = mq_open(SMS_APP_MQ_NAME, O_WRONLY | O_NONBLOCK);
    if (ctx->mq_tx == (mqd_t)-1) {
        fprintf(stderr, "sms_app: mq_open(send) failed: %s\n", strerror(errno));
        return -1;
    }
    return 0;
}

/* Safe to call with partially initialised resources. */
static void free_resources(sms_ctx_t *ctx)
{
    if (ctx->mq_tx != (mqd_t)-1)
        mq_close(ctx->mq_tx);
    if (ctx->mq_rx != (mqd_t)-1) {
        mq_close(ctx->mq_rx);
        mq_unlink(SMS_APP_MQ_NAME);
    }
    if (ctx->dev_fd >= 0)
        close(ctx->dev_fd);                                 /* no fd leak */
    logger_close(ctx);
    cbuf_destroy(ctx->sensor_buf);
    cbuf_destroy(ctx->log_buf);
    free(ctx->jitter_us);
}

static int start_thread(pthread_t *tid, void *(*fn)(void *), void *arg)
{
    pthread_attr_t attr;
    int rc;

    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, SMS_APP_STACK_SIZE);
    rc = pthread_create(tid, &attr, fn, arg);
    pthread_attr_destroy(&attr);
    return rc;
}

/* Blocks until SIGINT/SIGTERM, duration elapsed, or a fatal error. */
static void wait_for_end(sms_ctx_t *ctx)
{
    sigset_t        waitset;
    struct timespec slice = { 0, 200L * 1000000L };

    sigemptyset(&waitset);
    sigaddset(&waitset, SIGINT);
    sigaddset(&waitset, SIGTERM);

    while (atomic_load(&ctx->running)) {
        int sig = sigtimedwait(&waitset, NULL, &slice);

        if (sig == SIGINT || sig == SIGTERM) {
            fprintf(stderr, "\n[SMS] signal %d received, shutting down...\n", sig);
            logger_post(ctx, "INFO", "signal %d received", sig);
            return;
        }
        if (ctx->cfg.duration_s != 0 &&
            sms_monotonic_ms() - ((long long)ctx->start_ts.tv_sec * 1000LL +
                                  ctx->start_ts.tv_nsec / 1000000L) >=
                (long long)ctx->cfg.duration_s * 1000LL) {
            logger_post(ctx, "INFO", "duration of %u s elapsed", ctx->cfg.duration_s);
            return;
        }
    }
}

int main(int argc, char **argv)
{
    sms_ctx_t        ctx;
    app_state_t      state = STATE_INIT;
    pthread_t        t_sensor, t_proc, t_alert, t_log;
    int              started_sensor = 0, started_proc = 0, started_alert = 0, started_log = 0;
    sigset_t         blockset;
    struct sigaction sa;
    unsigned long    total_alerts = 0;
    long long        uptime_ms;
    int              i, rc = 0;

    memset(&ctx, 0, sizeof(ctx));
    ctx.dev_fd = -1;
    ctx.mq_tx  = (mqd_t)-1;
    ctx.mq_rx  = (mqd_t)-1;

    if (parse_args(argc, argv, &ctx.cfg) != 0) {
        usage(argv[0]);
        return EXIT_USAGE;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);                       /* -v output appears line by line */

    /* Signals: SIGINT/SIGTERM are consumed synchronously by main (sigtimedwait);
     * SIGUSR1 interrupts the Sensor thread. All three blocked before creating threads. */
    sigemptyset(&blockset);
    sigaddset(&blockset, SIGINT);
    sigaddset(&blockset, SIGTERM);
    sigaddset(&blockset, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &blockset, NULL);
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigusr1_handler;                        /* no SA_RESTART on purpose */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    /* Avoid page faults breaking the timing of the real-time thread. */
    if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
        fprintf(stderr, "[SMS] WARNING: mlockall failed: %s\n", strerror(errno));

    clock_gettime(CLOCK_MONOTONIC, &ctx.start_ts);
    atomic_store(&ctx.running, 1);

    if (init_resources(&ctx) != 0) {
        free_resources(&ctx);
        return EXIT_RUNTIME;
    }

    logger_post(&ctx, "INFO", "SMS start: interval=%ums temp_thr=%.1f hum_thr=%.1f duration=%us dev=%s",
                ctx.cfg.interval_ms, (double)ctx.cfg.temp_threshold, (double)ctx.cfg.hum_threshold,
                ctx.cfg.duration_s, ctx.cfg.dev_path);

    /* ---- start the 4 threads (consumers first, producer last) ---- */
    if (start_thread(&t_log, logger_thread, &ctx) == 0)       started_log = 1;
    if (started_log && start_thread(&t_alert, alert_thread, &ctx) == 0)       started_alert = 1;
    if (started_alert && start_thread(&t_proc, processor_thread, &ctx) == 0)  started_proc = 1;
    if (started_proc && start_thread(&t_sensor, sensor_thread, &ctx) == 0)    started_sensor = 1;

    if (!started_sensor) {
        fprintf(stderr, "sms_app: failed to start threads\n");
        atomic_store(&ctx.fatal_error, 1);
        atomic_store(&ctx.running, 0);
    } else {
        change_state(&ctx, &state, STATE_RUNNING);
        wait_for_end(&ctx);
    }

    /* ---- graceful shutdown ---- */
    change_state(&ctx, &state, STATE_SHUTDOWN);
    atomic_store(&ctx.running, 0);

    if (started_sensor) {                                   /* 1. stop the producer */
        pthread_kill(t_sensor, SIGUSR1);
        pthread_join(t_sensor, NULL);
    }
    cbuf_close(ctx.sensor_buf);                             /* 2. let Processor drain, then exit */
    if (started_proc)
        pthread_join(t_proc, NULL);
    if (started_alert) {                                    /* 3. wake the Alert thread */
        if (alert_send_stop(&ctx) != 0)
            fprintf(stderr, "[SMS] WARNING: could not send stop message to Alert thread\n");
        pthread_join(t_alert, NULL);
    }

    uptime_ms = sms_monotonic_ms() - ((long long)ctx.start_ts.tv_sec * 1000LL +
                                      ctx.start_ts.tv_nsec / 1000000L);
    for (i = 0; i < ALERT_TYPE_COUNT; i++)
        total_alerts += atomic_load(&ctx.stats.alerts[i]);

    logger_post(&ctx, "SUMMARY", "samples read=%lu processed=%lu overflow=%lu read_errors=%lu",
                atomic_load(&ctx.stats.samples_read), atomic_load(&ctx.stats.samples_processed),
                cbuf_overflow_count(ctx.sensor_buf), atomic_load(&ctx.stats.read_errors));
    logger_post(&ctx, "SUMMARY", "alerts TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu total=%lu",
                atomic_load(&ctx.stats.alerts[ALERT_TEMP_HIGH]),
                atomic_load(&ctx.stats.alerts[ALERT_HUMID_HIGH]),
                atomic_load(&ctx.stats.alerts[ALERT_TEMP_SPIKE]), total_alerts);
    logger_post(&ctx, "SUMMARY", "uptime=%.1fs, exit=%s", (double)uptime_ms / 1000.0,
                atomic_load(&ctx.fatal_error) ? "ERROR" : "OK");

    cbuf_close(ctx.log_buf);                                /* 4. Logger drains, flushes, exits */
    if (started_log)
        pthread_join(t_log, NULL);

    /* ---- summary on the terminal ---- */
    printf("\n========== SMS summary ==========\n");
    printf("Uptime            : %.1f s\n", (double)uptime_ms / 1000.0);
    printf("Samples read      : %lu\n", atomic_load(&ctx.stats.samples_read));
    printf("Samples processed : %lu\n", atomic_load(&ctx.stats.samples_processed));
    printf("Buffer            : produced=%lu consumed=%lu overflow=%lu\n",
           cbuf_total_produced(ctx.sensor_buf), cbuf_total_consumed(ctx.sensor_buf),
           cbuf_overflow_count(ctx.sensor_buf));
    printf("Alerts            : TEMP_HIGH=%lu HUMID_HIGH=%lu TEMP_SPIKE=%lu (total %lu, dropped %lu)\n",
           atomic_load(&ctx.stats.alerts[ALERT_TEMP_HIGH]),
           atomic_load(&ctx.stats.alerts[ALERT_HUMID_HIGH]),
           atomic_load(&ctx.stats.alerts[ALERT_TEMP_SPIKE]), total_alerts,
           atomic_load(&ctx.stats.alarms_dropped));
    printf("Read errors       : %lu   Period overruns: %lu\n",
           atomic_load(&ctx.stats.read_errors), atomic_load(&ctx.stats.overruns));
    printf("Log messages lost : %lu\n", cbuf_overflow_count(ctx.log_buf));
    printf("Log file          : %s\n", ctx.cfg.log_path);

    if (ctx.jitter_count > 0 && sensor_write_jitter_report(&ctx) == 0)
        printf("Jitter report     : %s (%zu samples)\n", ctx.cfg.jitter_path, ctx.jitter_count);

    rc = atomic_load(&ctx.fatal_error) ? EXIT_RUNTIME : EXIT_SUCCESS;
    free_resources(&ctx);
    return rc;
}
