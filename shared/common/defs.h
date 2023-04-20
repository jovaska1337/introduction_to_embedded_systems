#ifndef DEFS_H
#define DEFS_H

#define UNO  0
#define MEGA 1

// platform
#if defined(__AVR_ATmega2560__)
#define PLATFORM MEGA
#elif defined(__AVR_ATmega328P__)
#define PLATFORM UNO
#else
#error "Invalid platform."
#endif

// both boards have 16MHz oscillators
#define F_CPU 16000000UL

// array size at compile time
#define length(A) (sizeof(A)/sizeof(*(A)))

#endif //!DEFS_H
