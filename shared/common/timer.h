#ifndef TIMER_H
#define TIMER_H

#include "util/type.h"

// stop timer
void timer_stop();

// start timer
void timer_start();

// setup timer for frequency
void timer_setup(u16 freq);

#endif // !TIMER_H
