#ifndef INTERRUPT_H
#define INTERRUPT_H

#include <avr/io.h>
#include <avr/interrupt.h>

// save interrupt mask and disable interrupts
#define save_int() u8 __SREG = SREG; cli()

// restore interrupt mask (must be paired with no_int())
#define rest_int() SREG = __SREG

#endif
