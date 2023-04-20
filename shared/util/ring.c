#include "util/ring.h" 
#include "util/memory.h"
#include "util/interrupt.h"

#include <string.h>

ptr ring_put(ring_t *buf, ptr src, u8 flags)
{
	u8 *dst;

	save_int();

	// buffer is full
	if (buf->count > buf->mask) {
		dst = NULL;
		goto end;
	}

	// write destination
	dst = &buf->data[buf->pos*buf->unit];

	// wrap position
	buf->pos = (buf->pos + 1) & buf->mask;

	// new item
	buf->count++;

	(void) copy(dst, src, buf->unit, flags);
end:
	rest_int();

	return dst;
}

ptr ring_pop(ring_t *buf, ptr dst)
{
	u8 *src;

	save_int();

	// no items
	if (buf->count < 1) {
		dst = NULL;
		goto end;
	}

	src = &buf->data[((buf->pos - buf->count) & buf->mask)*buf->unit];

	// copy to dst (if specified)
	if (dst != NULL)
		(void) memcpy(dst, src, buf->unit);
	else
		dst = src;

	// one item removed
	buf->count--;
end:
	rest_int();

	return dst;
}

ptr ring_get(ring_t *buf, u8 n)
{
	u8 *src;

	save_int();

	// no items
	if (buf->count < 1) {
		src = NULL;
		goto end;
	}

	// real index
	src = &buf->data[((buf->pos - buf->count + n) & buf->mask)*buf->unit];
end:
	rest_int();

	return src;
}
