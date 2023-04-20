#include "globals.h"
#include "util/init.h"
#include "util/memory.h"
#include "util/interrupt.h"
#include "common/defs.h"
#include "program/alarm.h"

/* timer prescalers and their bits in TCCR0B */
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

static u8 bits, count, ticks;

void alarm_set(u8 disable)
{
	save_int();

	if (disable) {
		// disable timer
		TCCR0B &= ~7;

		// disable modulator
		ticks = 0;
		ev_set_id(ALARM_TIMER, 1);

		// disable digital output
		PIND &= ~_BV(7);
	} else {
		// enable timer
		TCCR0B |= bits;
		OCR0A   = count;

		// enable modulator
		ticks = ALARM_MODULATE_TICKS;
		ev_set_id(ALARM_TIMER, 0);
		
		// enable digital output
		PIND |= _BV(7);
	}

	rest_int();
}

void alarm_freq(u16 freq)
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
		now = F_CPU/(2*(u32)freq*now) - 1;
		if (now > (u8)~0)
			break;
		old = now;
		i++;	
	}
	i -= !!i;

	// set globals
	bits  = rom(ps[i].bits, byte);
	count = old;

	rest_int();
}

u8 e_alarm_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	if (ticks-- > 0)
		return 0;

	// we have a capacitor on the buzzer, so it doesn't
	// matter if the timer leaves the pin high
	if (TCCR0B & 7) {
		TCCR0B = 0;
	} else {
		TCCR0B = bits;
		OCR0A  = count;
	}

	ticks = ALARM_MODULATE_TICKS;

	return 0;
}

INIT()
{
	// D6 & D7 to output mode
	DDRD  |=   _BV(6) | _BV(7);
	PORTD &= ~(_BV(6) | _BV(7));

	// CTC mode on TIMER0 port A (D6)
	TCCR0A |= _BV(COM0A0) | _BV(WGM01);
}
