#ifndef ATTR_H
#define ATTR_H

// GCC attributes
#define noreturn    __attribute__((noreturn))
#define unused      __attribute__((unused))
#define _used       __attribute__((used))
#define naked       __attribute__((naked))
#define packed      __attribute__((packed))
#define may_alias   __attribute__((__may_alias__))
#define fallthrough __attribute__((fallthrough))
#define unreachable __builtin_unreachable()

// GCC optimization for conditionals
#define likely(A)   __builtin_expect((A), 1)
#define unlikely(A) __builtin_expect((A), 0)

#endif // !ATTR_H
