#include "util/init.h"
#include "util/type.h"
#include "util/attr.h"
#include "common/defs.h"

#include <avr/io.h>

u8 e_tick_show(u8 unused id, u8 unused code, ptr unused arg)
{
#if PLATFORM == MEGA
	PIND |= _BV(0);
#elif PLATFORM == UNO
	PIND |= _BV(2);
#endif
	return 0;
}

INIT(7)
{
#if PLATFORM == MEGA
	DDRD  |=  _BV(0);
	PORTD |= ~_BV(0);
#elif PLATFORM == UNO
	DDRD  |=  _BV(2);
	PORTD &= ~_BV(2);
#endif
}

