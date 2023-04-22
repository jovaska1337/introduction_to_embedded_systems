#ifndef GLOBALS_H
#define GLOBALS_H

#include "util/event.h"
#include "common/defs.h"

// how many events are buffered, increase if unstable
#define EVENT_BUFSIZE 5 // (1 << 5) = 32

/* Due to severe memory limitations and architectural limitations on both
 * ATMega328 and ATMega2560, we don't have the luxury of implementing proper
 * encapsulation. Thus these global variables are REQUIRED to properly
 * implement this kind of event driven program structure.
 */

extern event_loop_t g_event_loop; // global event loop

// event codes
typedef enum {
#define _C_(code) code,
#define _H_(id, handler, code, disable)
#include "globals.in"
#undef _C_
#undef _H_
} _event_code_t;

// event handler ids
typedef enum {
#define _C_(code)
#define _H_(id, handler, code, disable) id,
#include "globals.in"
#undef _C_
#undef _H_
} _handler_id_t;

// macro trickery to allow dispatch() to have
// the argument parameter as optional (NULL)
#define __dispatch(code, arg) ({ \
	event_t __event = {(_event_code_t)code, (arg)}; \
	event_dispatch(&g_event_loop, &__event, 0); })
#define __dispatch_expand(a, b) __dispatch(a, b)
#define __dispatch_arg(a, b, ...) b
#define dispatch(code, ...) \
	__dispatch_expand(code, __dispatch_arg(,##__VA_ARGS__, NULL))

// wrappers for other event.c functions that apply
// to the global event loop structure
#define ev_set_id(id, disable) \
	event_set_id(&g_event_loop, (_handler_id_t)id, (disable))
#define ev_set_code(code, disable) \
	event_set_code(&g_event_loop, (_event_code_t)code, (disable))
#define ev_run() \
	event_run(&g_event_loop)

// I fully aknowledge that no having parentheses in the macro
// arguments above is dangerous but it can't be avoided here.

#endif // !GLOBALS_H
