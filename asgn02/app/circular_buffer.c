/*
 * circular_buffer.c
 * Purpose : Implementation of the thread-safe circular buffer.
 *           Locking rule: the only lock taken here is cb->lock and it is never
 *           held while calling any other function that takes a lock, so this
 *           module cannot take part in a deadlock cycle.
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#include "circular_buffer.h"

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct cbuf {
    unsigned char  *data;
    size_t          capacity;
    size_t          item_size;
    size_t          head;            /* next write index */
    size_t          tail;            /* next read index  */
    size_t          size;
    unsigned long   total_produced;
    unsigned long   total_consumed;
    unsigned long   overflow_count;
    int             closed;
    pthread_mutex_t lock;
    pthread_cond_t  not_empty;
};

cbuf_t *cbuf_create(size_t capacity, size_t item_size)
{
    cbuf_t *cb;
    pthread_condattr_t attr;

    if (capacity == 0 || item_size == 0)
        return NULL;

    cb = calloc(1, sizeof(*cb));
    if (!cb)
        return NULL;

    cb->data = calloc(capacity, item_size);   /* the one and only allocation of the storage */
    if (!cb->data) {
        free(cb);
        return NULL;
    }
    cb->capacity  = capacity;
    cb->item_size = item_size;

    pthread_mutex_init(&cb->lock, NULL);
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);   /* immune to wall-clock jumps */
    pthread_cond_init(&cb->not_empty, &attr);
    pthread_condattr_destroy(&attr);
    return cb;
}

void cbuf_destroy(cbuf_t *cb)
{
    if (!cb)
        return;
    pthread_cond_destroy(&cb->not_empty);
    pthread_mutex_destroy(&cb->lock);
    free(cb->data);
    free(cb);
}

/* caller holds cb->lock and cb->size > 0 */
static void take_locked(cbuf_t *cb, void *item)
{
    memcpy(item, cb->data + cb->tail * cb->item_size, cb->item_size);
    cb->tail = (cb->tail + 1) % cb->capacity;
    cb->size--;
    cb->total_consumed++;
}

int cbuf_push(cbuf_t *cb, const void *item)
{
    int rc = CBUF_OK;

    pthread_mutex_lock(&cb->lock);
    if (cb->closed) {
        rc = CBUF_CLOSED;
    } else if (cb->size == cb->capacity) {
        cb->overflow_count++;
        rc = CBUF_FULL;
    } else {
        memcpy(cb->data + cb->head * cb->item_size, item, cb->item_size);
        cb->head = (cb->head + 1) % cb->capacity;
        cb->size++;
        cb->total_produced++;
        pthread_cond_signal(&cb->not_empty);
    }
    pthread_mutex_unlock(&cb->lock);
    return rc;
}

int cbuf_pop(cbuf_t *cb, void *item)
{
    int rc = CBUF_OK;

    pthread_mutex_lock(&cb->lock);
    while (cb->size == 0 && !cb->closed)
        pthread_cond_wait(&cb->not_empty, &cb->lock);   /* loop: guards spurious wake-ups */

    if (cb->size > 0)
        take_locked(cb, item);
    else
        rc = CBUF_CLOSED;                               /* closed and drained */
    pthread_mutex_unlock(&cb->lock);
    return rc;
}

int cbuf_pop_timeout(cbuf_t *cb, void *item, unsigned int timeout_ms)
{
    struct timespec deadline;
    int rc = CBUF_OK;

    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec  += (time_t)(timeout_ms / 1000u);
    deadline.tv_nsec += (long)(timeout_ms % 1000u) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec  += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    pthread_mutex_lock(&cb->lock);
    while (cb->size == 0 && !cb->closed) {
        if (pthread_cond_timedwait(&cb->not_empty, &cb->lock, &deadline) == ETIMEDOUT)
            break;
    }

    if (cb->size > 0)
        take_locked(cb, item);
    else
        rc = cb->closed ? CBUF_CLOSED : CBUF_TIMEOUT;
    pthread_mutex_unlock(&cb->lock);
    return rc;
}

void cbuf_close(cbuf_t *cb)
{
    pthread_mutex_lock(&cb->lock);
    cb->closed = 1;
    pthread_cond_broadcast(&cb->not_empty);
    pthread_mutex_unlock(&cb->lock);
}

size_t cbuf_current_size(cbuf_t *cb)
{
    size_t v;

    pthread_mutex_lock(&cb->lock);
    v = cb->size;
    pthread_mutex_unlock(&cb->lock);
    return v;
}

unsigned long cbuf_total_produced(cbuf_t *cb)
{
    unsigned long v;

    pthread_mutex_lock(&cb->lock);
    v = cb->total_produced;
    pthread_mutex_unlock(&cb->lock);
    return v;
}

unsigned long cbuf_total_consumed(cbuf_t *cb)
{
    unsigned long v;

    pthread_mutex_lock(&cb->lock);
    v = cb->total_consumed;
    pthread_mutex_unlock(&cb->lock);
    return v;
}

unsigned long cbuf_overflow_count(cbuf_t *cb)
{
    unsigned long v;

    pthread_mutex_lock(&cb->lock);
    v = cb->overflow_count;
    pthread_mutex_unlock(&cb->lock);
    return v;
}
