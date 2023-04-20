#ifndef SLEEP_H
#define SLEEP_H

#include <avr/sleep.h>
#include <avr/interrupt.h>

#ifndef sleep_bod_disable
#define sleep_bod_disable()
#endif

/* put the CPU into sleep mode */
#define sleep() \
	do { \
		cli(); \
		sleep_enable(); \
		sleep_bod_disable(); \
		sei(); \
		sleep_cpu(); \
		sleep_disable(); \
	} while (0)

#endif
