#include "util/memory.h"

#include <string.h>

ptr copy(ptr dst, const ptr src, u16 size, u8 flags)
{
	// memset to null
	if (flags & NILDATA)
		return memset(dst, 0, size);

	// src is null terminated (C string)
	if (flags & NILTERM) {
		char *(*cpy)(char *, const char *);
		if (flags & ROMDATA)
			cpy = &strcpy_P;
		else
			cpy = &strcpy;
		return cpy(dst, src);
	
	// data with known size
	} else {
		void *(*cpy)(void *, const void *, size_t);
		if (flags & ROMDATA)
			cpy = &memcpy_P;
		else
			cpy = &memcpy;
		return cpy(dst, src, size);
	}
}
