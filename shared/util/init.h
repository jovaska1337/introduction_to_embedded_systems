#ifndef INIT_H
#define INIT_H

#include "util/attr.h"

// we use the .init<x> sections (executed before main())
// to initialize all modules. this way, every compilation
// unit can specify it's own initialization code without
// having to copy it into main() and the linker will
// handle stiching it all together
#define __INIT(N) \
	static naked void __init##N() \
		__attribute__((section(".init" #N))); \
	static void _used __init##N()
#define __INIT_EXPAND(N) __INIT(N)
#define __INIT_ARG(N, M, ...) M
#define INIT(...) __INIT_EXPAND(__INIT_ARG(,##__VA_ARGS__, 7))

// usage is as follows (should be used to initialize registers)
// use section 7 for modules, main.c uses 6 and 8 for base init
#if 0
INIT() // 7 by default
{
	// initialization code here
}
#endif

#endif //!INIT_H
