#ifndef EVENT_H
#define EVENT_H

#include "util/attr.h"
#include "util/type.h"
#include "util/ring.h"
#include "util/memory.h"

// passed to dispatch() from stack
typedef struct {
	u8 code;
	ptr arg;
} packed event_t;

// stored in ROM (saves 4 bytes of RAM per handler)
typedef struct {
	u8 (*func)(u8, u8, ptr);
	u8 code;
	u8 id;
} packed _event_handler_t;

// stored in RAM
typedef struct {
	_event_handler_t *rom; // ROM data
	u8 disable;
} packed event_handler_t;

// stored in RAM
// manually initialized (no sensible way of writing a generic initializer)
typedef struct event_loop {
	ring_t *buffer; // event buffer
	u8 n_handlers; // number of handlers
	event_handler_t handlers[]; // handlers (compile time constant)
} packed event_loop_t;

// disable event handler (by id)
void event_set_id(event_loop_t *loop, u8 id, u8 disable);

// disable event handlers (by event code)
void event_set_code(event_loop_t *loop, u8 code, u8 disable);

// run an even loop (handle buffered events)
u8 event_run(event_loop_t *loop);

// dispatch event to an event loop
u8 event_dispatch(event_loop_t *loop, const event_t *event, u8 flags);

#endif // !EVENT_H
