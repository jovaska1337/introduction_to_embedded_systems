#include "globals.h"
#include "util/init.h"
#include "util/attr.h"
#include "common/defs.h"

#include <avr/pgmspace.h>

// event/timer handler extern declarations (to make them visible here)
#define _C_(code)
#define _H_(id, handler, code, disable) \
	extern u8 handler(u8, u8, ptr);
#include "globals.in"
#undef _C_
#undef _H_

// event handler ROM data (must be declared separately)
#define _C_(code)
#define _H_(id, handler, code, disable) \
	static const _event_handler_t _##id##_rom PROGMEM = \
		{&(handler), (code), (id)};
#include "globals.in"
#undef _C_
#undef _H_

// event loop ring buffer
static ring_t g_event_loop_buffer = ring_init(event_t, EVENT_BUFSIZE);

// event loop RAM data
event_loop_t g_event_loop = {
	&g_event_loop_buffer,
	// number of event handlers
#define _C_(code)
#define _H_(id, handler, code, disable) {},
	sizeof((event_handler_t[]){
#include "globals.in"
	})/sizeof(((event_handler_t[]){
#include "globals.in"
	})[0]),
#undef _C_
#undef _H_
	// event handlers
	{
#define _C_(code)
#define _H_(id, handler, code, disable) \
	{(_event_handler_t *)&_##id##_rom, (disable)},
// event handler list (flexible struct member)
// GCC allows flexible member init as an extension
#include "globals.in"
	}
#undef _C_
#undef _H_
};
