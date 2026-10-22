/*
 * circular_buffer.h
 * Purpose : Thread-safe fixed-size circular buffer (mutex + condition variable).
 *           Generic: stores copies of fixed-size items. Used for the
 *           shared_buffer (SensorData) and for the log queue (LogMsg).
 * Author  : Nguyen Ngoc Quang - SE202007
 * Date    : 2026-10-21
 */
#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include <stddef.h>

#define CBUF_OK        0
#define CBUF_TIMEOUT   1     /* pop_timeout: nothing arrived in time        */
#define CBUF_FULL     (-1)   /* push: buffer full, item dropped (overflow)  */
#define CBUF_CLOSED   (-2)   /* push: closed / pop: closed and fully drained */

typedef struct cbuf cbuf_t;

/* Allocates the buffer on the heap ONCE (capacity * item_size bytes). */
cbuf_t *cbuf_create(size_t capacity, size_t item_size);
void    cbuf_destroy(cbuf_t *cb);

/* Producer: never blocks. Returns CBUF_OK, CBUF_FULL (overflow_count++) or CBUF_CLOSED. */
int cbuf_push(cbuf_t *cb, const void *item);

/* Consumer: blocks while empty. Returns CBUF_OK, or CBUF_CLOSED when closed AND empty. */
int cbuf_pop(cbuf_t *cb, void *item);

/* Like cbuf_pop but gives up after timeout_ms (returns CBUF_TIMEOUT). */
int cbuf_pop_timeout(cbuf_t *cb, void *item, unsigned int timeout_ms);

/* No more pushes; wakes every blocked consumer (they drain what is left, then get CBUF_CLOSED). */
void cbuf_close(cbuf_t *cb);

/* Getters */
size_t        cbuf_current_size(cbuf_t *cb);
unsigned long cbuf_total_produced(cbuf_t *cb);
unsigned long cbuf_total_consumed(cbuf_t *cb);
unsigned long cbuf_overflow_count(cbuf_t *cb);

#endif /* CIRCULAR_BUFFER_H */
