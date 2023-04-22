#ifndef STATE_H
#define STATE_H

#include "util/attr.h"
#include "common/defs.h"

// state change event data
typedef struct {
	enum { SHARED, INTERNAL } type;
	u8 now;
	u8 old;
} packed stev_t;

// shared state (synced over serial by backend)
typedef enum {
	INIT, // uninitialized
	ARMD, // waiting for motion
	ALRT, // motion detected
	ALRM, // alarm on
	ULCK  // unlocked, alarm off
} packed sstate_t;

#if PLATFORM == MEGA

// frontend internal state
typedef enum {
	BOOT, // boot animation
	LINK, // no link
	CODE, // code prompt
	MENU, // main menu
	IDLE  // idle state
} packed istate_t;

#elif PLATFORM == UNO

// backend internal state
typedef enum {
	BOOT, // booting up
	IDLE  // waiting for events
} packed istate_t;

#endif

#endif // !STATE_H
