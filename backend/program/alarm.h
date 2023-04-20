#ifndef ALARM_H
#define ALARM_H

#include "util/type.h"

#define ALARM_MODULATE_TICKS 1

// set alarm status
void alarm_set(u8 disable);

// set buzzer frequency
void alarm_freq(u16 freq);

#endif // !ALARM_H
