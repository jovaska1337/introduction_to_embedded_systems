#ifndef TYPE_H
#define TYPE_H

#include <stdint.h> // int sizes
#include <stddef.h> // NULL

// short names for standard types for known sizes
typedef float    f16;
typedef double   f32;
typedef uint8_t  u8;
typedef  int8_t  s8;
typedef uint16_t u16;
typedef  int16_t s16;
typedef uint32_t u32;
typedef  int32_t s32;
typedef void *   ptr;
typedef char     chr;
typedef char *   str;

// GCC type inference (for reducing mess)
#define infer __auto_type

#endif
