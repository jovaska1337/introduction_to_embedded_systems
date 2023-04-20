#ifndef BUTTON_H
#define BUTTON_H

// these tick values need to be retuned if the global
// timer frequency is altered. (10Hz at the moment)

// how many ticks to poll for button state
#define BTN_POLL_TICKS 2

// how many ticks can the state differ from a locked
// state before it's registered as a new event
#define BTN_JITTER_TICKS 1

// how many ticks until the hold state is activated
// (triggers a HOLD event and when released HLUP)
#define BTN_HOLD_TICKS 9

// how many ticks after an event until a new event
// can be registered
#define BTN_INACTIVE_TICKS 1

// key names (use as mask for event state)
#define K0 (1UL <<  8)
#define K1 (1UL << 15)
#define K2 (1UL << 11)
#define K3 (1UL <<  7)
#define K4 (1UL << 14)
#define K5 (1UL << 10)
#define K6 (1UL <<  6)
#define K7 (1UL << 13)
#define K8 (1UL <<  9)
#define K9 (1UL <<  5)
#define KA (1UL <<  3)
#define KB (1UL <<  2)
#define KC (1UL <<  1)
#define KD (1UL <<  0)
#define KS (1UL << 12)
#define KH (1UL <<  4)

// event types (use as mask for event state)
#define DOWN (1UL << 28)
#define HOLD (1UL << 29)
#define UP   (1UL << 30)
#define HLUP (1UL << 31)

// convert button event to character
chr ktoc(u32 state);

// convert button event to number
u8 kton(u32 state);

#endif // !BUTTON_H
