#ifndef MOTION_H
#define MOTION_H

#include "util/type.h"

// motion sensor debounce
#define MOTION_WAIT_TICKS 2

// set motion detector state
void motion_set(u8 disable);

#endif // !MOTION_H
