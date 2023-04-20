#include "globals.h"
#include "util/init.h"
#include "util/interrupt.h"
#include "program/motion.h"

static u8 ticks;

void motion_set(u8 disable)
{
	save_int();

	if (disable) {
		// disable interrupt
		PCMSK2 &= ~_BV(PCINT19);

		// reset debounce timer
		ticks = 0;
		ev_set_id(MOTION_TIMER, 1);
	} else {
		// enable interrupt
		PCMSK2 |= _BV(PCINT19);
	}

	rest_int();
}

ISR(PCINT2_vect)
{
	// we don't care about the high transition
	if (PIND & _BV(3))
		return;
	
	// low transition means motion
	dispatch(MOTION);

	// re-enable ISR with timer
	PCMSK2 &= ~_BV(PCINT19);
	ticks = MOTION_WAIT_TICKS;
	ev_set_id(MOTION_TIMER, 0);
}

u8 e_motion_timer(u8 id, u8 unused code, ptr unused arg)
{
	if (ticks-- > 0)
		return 0;
	
	// disable self
	ev_set_id(id, 1);

	// enable ISR
	PCMSK2 |= _BV(PCINT19);

	return 0;
}

INIT()
{
	// enable interrupts on PORTD
	PCICR |= _BV(PCIE2);
}
