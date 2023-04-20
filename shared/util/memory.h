#ifndef MEMORY_H
#define MEMORY_H

#include "util/type.h"

#include <avr/pgmspace.h>

// pgm_read_*() helper
#define rom(from, as) ((typeof(from))pgm_read_##as(&(from)))

#define ROMDATA (1 << 0) // data is in ROM
#define NILDATA (1 << 1) // data is NULL
#define NILTERM (1 << 2) // src is null terminated

// generic copy
ptr copy(ptr dst, const ptr src, u16 size, u8 flags);

#endif // !MEMORY_H
