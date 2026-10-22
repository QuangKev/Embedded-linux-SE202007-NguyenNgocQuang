/*
 * processor_thread.c
 * Purpose : Processor thread.
 *           - blocks on shared_buffer (condition variable) until a sample arrives
 *           - rolling average of temperature/humidity over the last 10 samples
 *           - raises TEMP_HIGH / HUMID_HIGH / TEMP_SPIKE alarms via the POSIX mq
 *           - sends every processed sample to the Logger (log queue)
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "processor_thread.h"
#include "logger_thread.h"

#include <errno.h>
#include <string.h>

/* Non-blocking send: if the alarm queue is full the alarm is dropped and counted. */
static void raise_alarm(sms_ctx_t *ctx, int type, long long ts_ms, float value, float threshold)
{
    AlarmMsg m;

    memset(&m, 0, sizeof(m));          /* struct padding must not leak uninitialised bytes into the mq */
    m.type         = type;
    m.timestamp_ms = ts_ms;
    m.value        = value;
    m.threshold    = threshold;

    if (mq_send(ctx->mq_tx, (const char *)&m, sizeof(m), 0) != 0)
        atomic_fetch_add(&ctx->stats.alarms_dropped, 1);
}

void *processor_thread(void *arg)
{
    sms_ctx_t *ctx = arg;
    SensorData sd;
    float      tw[SMS_APP_WINDOW_SIZE] = { 0 };    /* rolling window: temperature */
    float      hw[SMS_APP_WINDOW_SIZE] = { 0 };    /* rolling window: humidity    */
    unsigned   count = 0, idx = 0, i;
    float      prev_temp = 0.0f;
    int        have_prev = 0;

    while (cbuf_pop(ctx->sensor_buf, &sd) == CBUF_OK) {
        float avg_t = 0.0f, avg_h = 0.0f, delta;

        /* --- rolling average over the last SMS_APP_WINDOW_SIZE samples --- */
        tw[idx] = sd.temperature;
        hw[idx] = sd.humidity;
        idx = (idx + 1u) % SMS_APP_WINDOW_SIZE;
        if (count < SMS_APP_WINDOW_SIZE)
            count++;
        for (i = 0; i < count; i++) {
            avg_t += tw[i];
            avg_h += hw[i];
        }
        avg_t /= (float)count;
        avg_h /= (float)count;

        atomic_fetch_add(&ctx->stats.samples_processed, 1);

        /* --- anomaly detection --- */
        if (sd.temperature > ctx->cfg.temp_threshold)
            raise_alarm(ctx, ALERT_TEMP_HIGH, sd.timestamp_ms, sd.temperature,
                        ctx->cfg.temp_threshold);

        if (sd.humidity > ctx->cfg.hum_threshold)
            raise_alarm(ctx, ALERT_HUMID_HIGH, sd.timestamp_ms, sd.humidity,
                        ctx->cfg.hum_threshold);

        if (have_prev) {
            delta = sd.temperature - prev_temp;
            if (delta < 0.0f)
                delta = -delta;
            if (delta > SMS_APP_TEMP_SPIKE_DELTA)
                raise_alarm(ctx, ALERT_TEMP_SPIKE, sd.timestamp_ms, delta,
                            SMS_APP_TEMP_SPIKE_DELTA);
        }
        prev_temp = sd.temperature;
        have_prev = 1;

        /* --- processed sample -> Logger Thread --- */
        logger_post(ctx, "SAMPLE", "ts=%lld T=%.1f H=%.1f avgT=%.2f avgH=%.2f",
                    sd.timestamp_ms, (double)sd.temperature, (double)sd.humidity,
                    (double)avg_t, (double)avg_h);

        if (ctx->cfg.verbose)
            printf("[SAMPLE] ts=%lld T=%.1fC H=%.1f%% avgT=%.2f avgH=%.2f buf=%zu\n",
                   sd.timestamp_ms, (double)sd.temperature, (double)sd.humidity,
                   (double)avg_t, (double)avg_h, cbuf_current_size(ctx->sensor_buf));
    }
    return NULL;
}
