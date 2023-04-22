#include "util/event.h"
#include "util/memory.h"
#include "util/interrupt.h"

// disable event handler (by id)
void event_set_id(event_loop_t *loop, u8 id, u8 disable)
{
	u8 n;

	save_int();

	// number of handlers
	n = loop->n_handlers;
	for (u8 i = 0; i < n; i++)
	{
		event_handler_t *h = &loop->handlers[i];

		// assume duplicate id's don't exist (they shouldn't)
		if (unlikely(rom(h->rom->id, word) == id)) {
			h->disable = disable;
			break;
		}
	}

	rest_int();
}

// disable event handlers (by event code)
void event_set_code(event_loop_t *loop, u8 code, u8 disable)
{
	u8 n;

	save_int();

	// number of handlers
	n = loop->n_handlers;
	for (u8 i = 0; i < n; i++)
	{
		event_handler_t *h = &loop->handlers[i];

		// event code match
		if (unlikely(rom(h->rom->code, word) == code))
			h->disable = disable;
	}

	rest_int();
}

// run an event loop (handle buffered events)
u8 event_run(event_loop_t *loop)
{
	event_t ev;
	u8 p = 0;

	save_int();

	// loop through event buffer
	while (ring_pop(loop->buffer, &ev) != NULL)
	{
		// call event handlers
		for (u8 i = 0; i < loop->n_handlers; i++)
		{
			event_handler_t *h = &loop->handlers[i];

			// event handler is disabled
			if (h->disable)
				continue;

			// call event handler if event code matches
			if (unlikely(
				rom(h->rom->code, word) == ev.code
			)) {
				sei(); // enable interrupts

				// run with interrupts enabled
				u8 r = rom(h->rom->func, ptr)(
					rom(h->rom->id, word),
					ev.code, ev.arg);

				cli(); // disable interrupts

				// prevent other handlers from being run
				if (r)
					break;
			}
		}

		p++; // increment no. of handled events
	}

	rest_int();

	return p;
}

// dispatch event to an event loop
u8 event_dispatch(event_loop_t *loop, const event_t *event, u8 flags)
{
	// add event to buffer (may be discarded)
	return ring_put(loop->buffer, (ptr)event, flags) == NULL;
}
