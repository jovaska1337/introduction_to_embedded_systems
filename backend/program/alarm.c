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

void buzzer_set(u16 freq, u8 flags)
{
	u32 old, now;
	u8 i;

	save_int();

	// turn buzzer off
	if (flags == BUZOFF) {
		TCCR0B &= ~7;
		goto end;
	}

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

	// enable timer
	TCCR0B |= rom(ps[i].bits, byte);
	OCR0A   = old;

	// enablke modulator
	if (flags & BUZMOD) {
		// set globals
		bits  = TCCR0B & 7;
		count = OCR0A;

		// start timer
		ticks = BUZMOD_TICKS;
		ev_set_id(ALARM_TIMER, 0);
	}
end:
	rest_int();
}

void alarm_set(u8 disable)
{
	save_int();

	if (disable) {
		// disable buzzer
		buzzer_set(0, BUZOFF);

		// disable digital output
		PIND &= ~_BV(7);
	} else {
		// enable buzzer
		buzzer_set(ALARM_FREQ, BUZON | BUZMOD);
		
		// enable digital output
		PIND |= _BV(7);
	}

	rest_int();
}

u8 e_alarm_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	if (ticks-- > 0)
		return 0;
	ticks = BUZMOD_TICKS;

	// we have a capacitor on the buzzer, so it doesn't
	// matter if the timer leaves the pin high
	if (TCCR0B & 7) {
		TCCR0B = 0;
	} else {
		TCCR0B = bits;
		OCR0A  = count;
	}

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
