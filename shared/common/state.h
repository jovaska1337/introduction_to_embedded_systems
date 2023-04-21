#ifndef STATE_H
#define STATE_H

#include "util/attr.h"
#include "common/defs.h"

// state change event data
typedef struct {
	// avoid warning
#if PLATFORM == MEGA
	enum { SHARED, UI, INTERNAL } type;
#elif PLATFORM == UNO
	enum { SHARED, INTERNAL } type;
#endif
	u8 now;
	u8 old;
} packed stev_t;

// shared state (synced over serial)
typedef enum {
	INIT, // uninitialized
	ARMD, // waiting for motion
	ALRT, // motion detected
	ALRM, // alarm on
	ULCK, // unlocked, alarm off
} packed sstate_t;

#if PLATFORM == MEGA

// frontend internal state
typedef enum {
	BOOT, // booting up
	LINK, // waiting for link
	WAIT  // waiting for input
} packed istate_t;

// frontend UI state
typedef enum {
	ANIM, // boot animation
	NLNK, // no link
	CODE, // code prompt
	MENU, // main menu
	IDLE  // idle state
} packed uistate_t;

#elif PLATFORM == UNO

// backend internal state
typedef enum {
	BOOT, // booting up
	WAIT  // waiting for events
} packed istate_t;

#endif

#endif // !STATE_H
