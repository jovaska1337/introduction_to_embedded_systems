#include "globals.h"
#include "util/init.h"
#include "util/memory.h"
#include "util/interrupt.h"
#include "program/button.h" 

#include <avr/cpufunc.h>

static const chr kchars[16] PROGMEM = {
	'D', 'C', 'B', 'A', '#', '9', '6', '3',
	'0', '8', '5', '2', '*', '7', '4', '1'
};

// convert button event to character
chr ktoc(u32 state)
{
	u8 index;

	// strip event type
	state &= 0xFFFF;

	// invalid state
	if (state == 0)
		return 0;

	// index in kchars
	index = __builtin_ctz(state);

	return rom(kchars[index], byte);
}

static const u8 knums[16] PROGMEM = {
	0xD, 0xC, 0xB, 0xA, 0x0, 0x9, 0x6, 0x3,
	0x0, 0x8, 0x5, 0x2, 0x0, 0x7, 0x4, 0x1
};

// convert button event to number
u8 kton(u32 state)
{
	u8 index;

	// strip event type
	state &= 0xFFFF;

	// invalid state
	if (state == 0)
		return 0;

	// index in knums
	index = __builtin_ctz(state);

	return rom(knums[index], byte);
}

/* internal state machine */
static volatile struct {
	struct {
		u16 state; // button state
		u16 flags; // other stuff
	} packed out;
	u8 t1; // tick counter 1
	u8 t2; // tick counter 2
#define WAIT 0
#define POLL 1
#define LOCK 2
#define IACT 3
	u8 state; // internal state
} packed state;

// PORTK pins (cast braindamage)
#define COLS ((u8)((u8)~0 >> 4))
#define ROWS ((u8)((u8)~0 << 4))

// scan buttons
static u16 scan()
{
	u16 result = 0;

	/* columns PD0-3 */
	for (u8 i = 0; i < 4; i++)
	{
		/* buttons pull the column down */
		if (PINK & _BV(i))
			continue;

		PORTK &= ~_BV(i); // disable pulldown

		/* rows PD4-PD7 */
		for (u8 j = 0; j < 4; j++)
		{
			PORTK = _BV(j + 4) | COLS; // set row high

			_NOP(); // wait for pin setup

			/* if column sees high, button is down */
			if (PINK & _BV(i)) {
				/* the keypad is indexed as follows
				 *   +---------+
				 *   | 1 2 3 A | 3
				 *   | 4 5 6 B | 2
				 *   | 7 8 9 C | 1
				 *   | * 0 # D | 0
				 *   +---------+ i
				 *     3 2 1 0 j
				 */
				result |= 1 << (4*i + j);
			}

			PORTK = COLS; // reset
		}
	}

	return result;
}

u8 e_button_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	save_int();

	/* button state machine */
	switch (state.state) {
	case WAIT:
		// the timer should never run here
		ev_set_id(BUTTON_TIMER, 1);
		break;
	
	case POLL:
		// wait for BTN_POLL_TICKS
		if (state.t1-- > 0)
			break;

		// dispatch DOWN event
		state.out.flags = DOWN >> 16;
		dispatch(BUTTON, (ptr)&state.out);

		// lock state
		state.t1 = 0x7F; // jitter counter
		state.t2 = BTN_HOLD_TICKS; // hold counter
		PCMSK2 = 0; // mask interrupts
		state.state = LOCK;
		break;
	
	case LOCK:
		// detected possible jitter
		if (state.t1 != 0x7F) {
			if (state.t1 < 1) {
				// state mismatch after jitter timeout
				if (scan() != state.out.state) {
					// if holding, dispatch HLUP
					if (state.out.flags & (HOLD >> 16))
						state.out.flags = HLUP >> 16;
					// otherwise dispatch UP
					else
						state.out.flags = UP >> 16;
					dispatch(BUTTON, (ptr)&state.out);

					// inactive state
					state.t1 = BTN_INACTIVE_TICKS;
					state.state = IACT;
					break;

				// invalidate jitter counter
				} else {
					state.t1 = 0x7F;
				}
			} else {
				state.t1--;
			}
		} else {
			// state mismatch
			if (scan() != state.out.state)
				state.t1 = BTN_JITTER_TICKS; // start
		}
	
		// hold state
		if (state.t2 < 1) {
			// dispatch HOLD event
			state.out.flags = HOLD >> 16;
			dispatch(BUTTON, (ptr)&state.out);
			
			// invalidate
			state.t2 = 0x7F;
		} else {
			// decrement if not invalidated
			if (state.t2 != 0x7F)
				state.t2--;
		}

		break;
	
	case IACT:
		// wait for BTN_INACTIVE_TICKS
		if (state.t1-- > 0)
			break;

		// wait state
		ev_set_id(BUTTON_TIMER, 1);
		PCMSK2 = COLS;
		state.state = WAIT;

		break;
	}

	rest_int();

	return 0;
}

// TODO: implement pin change interrupt cooldown with timer

ISR(PCINT2_vect)
{
	u16 temp;

	PCMSK2 = 0;

	switch (state.state) {
	case WAIT:
		// scan buttons
		temp = scan();

		// invalid change
		if (temp == 0)
			goto out;
		
		// scan buttons
		state.out.state = temp;

		// start polling
		state.t1 = BTN_POLL_TICKS;
		state.state = POLL;

		// start timer
		ev_set_id(BUTTON_TIMER, 0);
out:
		PCMSK2 = COLS;
		break;
	
	case POLL:
		// scan and OR with current state (debounce)
		state.out.state |= scan();
		PCMSK2 = COLS;
		break;
	}
}

INIT()
{
	DDRK   = ROWS; // rows -> output, cols -> input
	PORTK  = COLS; // rows -> low   , cols -> pullup
	PCMSK2 = COLS; // pin change interrupts for cols
	PCICR |= _BV(PCIE2);
}
