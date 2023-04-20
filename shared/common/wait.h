#ifndef WAIT_H
#define WAIT_H

#include "common/defs.h" // CPU_FREQ
#include <util/delay.h>

// these should be avoided
#define wait_ms(ms) _delay_ms(ms)
#define wait_us(us) _delay_us(us)

#endif // !WAIT_H
