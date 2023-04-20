#include "globals.h"
#include "util/init.h"
#include "util/sleep.h"
#include "common/defs.h"
#include "common/timer.h"

#include <avr/io.h>

/* base initialization, modules should use INIT() without
 * parameters for their initialization to execute after this
 */
INIT(1)
{
	/* setup power reduction (power down unused modules)
	 * Mega (Frontend):
	 *  TWI    -> unused
	 *  Timer2 -> unused
	 *  Timer0 -> unused
	 *  Timer1 -> for global tick timer
	 *  SPI    -> unused
	 *  USART0 -> unused (arduino uses this for USB serial)
	 *  ADC    -> unused
	 *  Timer5 -> unused
	 *  Timer4 -> unused
	 *  Timer3 -> unused
	 *  USART3 -> unused
	 *  USART2 -> unused
	 *  USART1 -> for serial communication
	 * UNO (Backend):
	 *  TWI    -> unused
	 *  Timer2 -> unused
	 *  Timer0 -> for tone generation
	 *  Timer1 -> for global tick timer
	 *  SPI    -> unused
	 *  USART  -> for serial communication
	 *  ADC    -> unused
	 */
#if PLATFORM == MEGA
	PRR0 = _BV(PRTWI)  | _BV(PRTIM2) | _BV(PRTIM0) | _BV(PRSPI)    | _BV(PRUSART0) | _BV(PRADC);
	PRR1 = _BV(PRTIM5) | _BV(PRTIM4) | _BV(PRTIM3) | _BV(PRUSART3) | _BV(PRUSART2);
#elif PLATFORM == UNO
	PRR = _BV(PRTWI) | _BV(PRTIM2) | _BV(PRSPI) | _BV(PRADC);
#endif

	/* setup sleep (idle mode --> USART wakeup) */
	SMCR = 0; // sleep mode --> idle

	/* initialize IO ports into pullup mode (least power loss on unused pins) */
#if PLATFORM == MEGA
	DDRA  = DDRB  = DDRC  = DDRD  = DDRE  = DDRF  = DDRG  = DDRH  = DDRJ  = DDRK  = DDRL  =  0; // INPUT
	PORTA = PORTB = PORTC = PORTD = PORTE = PORTF = PORTG = PORTH = PORTJ = PORTK = PORTL = ~0; // PULLUP
#elif PLATFORM == UNO
	DDRB  = DDRC  = DDRD  =  0; // INPUT
	PORTB = PORTC = PORTD = ~0; // PULLUP
#endif
}

/* after module initialization */
INIT(8) {
	timer_setup(10); // 10Hz global timer
	timer_start();   // start timer
	sei();           // enable interrupts
}

int main()
{
main:
	/* main() runs the global event loop and puts the CPU into sleep
	 * if no events are coming in (ie. only possible event sources are
	 * interrupt handlers), thus this loop will run at least at the
	 * configured global timer interrupt frequency.
	 */
	if (unlikely(ev_run() < 1))
	    	sleep(); // sleep until interrupt
	goto main;
	unreachable;
}
