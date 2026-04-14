#pragma once

/*
 * ring_buffer.h
 *
 * A minimal lock-free SPSC (single-producer, single-consumer) byte ring buffer
 * safe for use between the two RP2040 cores.
 *
 * Core1 (USB host / PIO) is the producer — it writes bytes received from the PC.
 * Core0 (USB device)     is the consumer — it reads bytes and feeds the parser.
 *
 * The RP2040 guarantees that aligned 32-bit reads/writes are atomic, so
 * a single read index (owned by consumer) and single write index (owned by
 * producer) with a power-of-2 buffer gives a correct lock-free ring.
 *
 * A second ring (core0 → core1) carries ACK bytes back to the PC.
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "pico/platform.h"  // __force_inline, memory_barrier

#define RING_SIZE 512   /* must be a power of 2 */

typedef struct {
    uint8_t  buf[RING_SIZE];
    volatile uint32_t head;   /* written by producer */
    volatile uint32_t tail;   /* written by consumer */
} ring_buf_t;

/* Shared between cores — placed in normal SRAM (not scratch) */
extern ring_buf_t rx_ring;   /* core1 → core0: incoming GIMX bytes */
extern ring_buf_t tx_ring;   /* core0 → core1: outgoing ACK bytes  */

/* ---- producer API (called from the producer core only) ---- */

static inline bool ring_push(ring_buf_t *r, uint8_t byte)
{
    uint32_t next = (r->head + 1) & (RING_SIZE - 1);
    if (next == r->tail) return false;   /* full */
    r->buf[r->head] = byte;
    __dmb();                              /* ensure data written before head update */
    r->head = next;
    return true;
}

static inline uint32_t ring_push_buf(ring_buf_t *r, const uint8_t *data, uint32_t len)
{
    uint32_t pushed = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (!ring_push(r, data[i])) break;
        pushed++;
    }
    return pushed;
}

/* ---- consumer API (called from the consumer core only) ---- */

static inline bool ring_pop(ring_buf_t *r, uint8_t *out)
{
    if (r->tail == r->head) return false;  /* empty */
    *out = r->buf[r->tail];
    __dmb();
    r->tail = (r->tail + 1) & (RING_SIZE - 1);
    return true;
}

static inline bool ring_empty(const ring_buf_t *r)
{
    return r->tail == r->head;
}

static inline uint32_t ring_used(const ring_buf_t *r)
{
    return (r->head - r->tail) & (RING_SIZE - 1);
}
