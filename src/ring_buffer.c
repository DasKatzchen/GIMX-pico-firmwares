#include "ring_buffer.h"

/* Definitions of the two shared ring buffers */
ring_buf_t rx_ring = { .head = 0, .tail = 0 };
ring_buf_t tx_ring = { .head = 0, .tail = 0 };
