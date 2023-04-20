#ifndef RING_H
#define RING_H

#include "util/type.h"

// ring buffer (FIFO)
// (ROM wouldn't save a single byte, 2 const u8, pointer is u16)
typedef struct {
	u8 pos;    // position in buffer
	u8 unit;   // size of single item
	u8 count;  // number of items
	u8 mask;   // mask for ring
	u8 data[]; // actual buffer
} ring_t;

// initializer literal generator
#define ring_init(t, s) \
	{	.pos   = 0, \
		.unit  = sizeof(t), \
		.count = 0, \
		.mask  = (1 << (s)) - 1, \
		.data  = { [0 ... sizeof(t)*(1 << (s)) - 1] = 0 } }

#define ROMPTR (1 << 0)
#define NILPTR (1 << 1)

// put item into buffer
ptr ring_put(ring_t *buf, ptr src, u8 flags);

// pop item from buffer 
ptr ring_pop(ring_t *buf, ptr dst);

// get pointer to nth item
ptr ring_get(ring_t *buf, u8 n);

#endif // !RING_H
