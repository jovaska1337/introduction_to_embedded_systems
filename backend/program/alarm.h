#ifndef ALARM_H
#define ALARM_H

#include "util/type.h"

#define BUZMOD_TICKS 1
#define ALARM_FREQ 500

#define BUZOFF 0
#define BUZON  (1 << 0)
#define BUZMOD (1 << 2)

// control buzzer separately
void buzzer_set(u16 freq, u8 flags);

// set alarm status
void alarm_set(u8 disable);

#endif // !ALARM_H
