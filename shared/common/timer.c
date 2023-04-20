#include "globals.h"
#include "util/init.h"
#include "util/memory.h"
#include "util/interrupt.h"
#include "common/defs.h"
#include "common/timer.h"

/* for TIMER1 we don't need plaform specific #if defined()'s
 * as the registers and presaclers are identical for TIMER1
 * on ATMega328 and ATMega2560)
 */

/* timer prescalers and their bits in TCCR1B */
static const struct {
	u16 fact;
	u8  bits;
} packed ps[] PROGMEM = { 
	{1024, 5},
	{ 256, 4},
	{  64, 3},
	{   8, 2},
	{   1, 1}
};

/* stop timer */
void timer_stop()
{
	save_int();

	TIMSK1 &= ~_BV(OCIE1A); // disable timer

	rest_int();
}

/* start timer */
void timer_start()
{
	save_int();

	TIMSK1 |= _BV(OCIE1A); // enable timer

	rest_int();
}

/* setup timer frequency (select prescaler and compare match) */
void timer_setup(u16 freq)
{
	u32 old, now;
	u8 i;

	save_int();

	// select best prescaler (largest compare match value)
	i = 0;
	old = (u16)~0;
	while (i < length(ps))
	{
		now = rom(ps[i].fact, word); // temp
		now = F_CPU/((u32)freq*now) - 1;
		if (now > (u16)~0)
			break;
		old = now;
		i++;	
	}
	i -= !!i;

	// configure timer
	TCCR1B &= ~7;
	TCCR1B |= rom(ps[i].bits, byte);
	OCR1A   = old;

	rest_int();
}

/* we use the TIMER event to signal a timer interrupt */
ISR(TIMER1_COMPA_vect)
{
	dispatch(TIMER);
}

/* timer initialization (started by main.c) */
INIT()
{
	TCCR1B = _BV(WGM12); // CTC mode
}
