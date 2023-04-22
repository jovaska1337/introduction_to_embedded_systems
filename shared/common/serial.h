#ifndef SERIAL_H
#define SERIAL_H

#include "util/type.h"
#include "util/attr.h"
#include "util/ring.h"
#include "common/state.h"

// this packet system is trash, it should be redesigned

// how many packets to buffer
#define SERIAL_BUFSIZE 3 // (1 << 3) = 8

// packet sent between frontend and backend
typedef struct {
	// packet type enumeration
	enum {
		SYNC,    // synchronize state
		CHANGE,  // state change
		CHKCODE, // unlock with code
		NEWCODE  // change unlock code
	} packed type;

	// message mode
	enum { REQUEST, RESPONSE, MESSAGE } packed mode;

	// mode specific header
	union {
		struct {
			u8 dummy[0]; // empty
		} packed request;

		struct {
			enum { OK, FAIL } packed status;
		} packed response;

		struct {
			u8 dummy[0]; // empty
		} packed message;
	} packed header;

	// packet body
	union {
		struct {
			sstate_t now;
		} packed sync;

		struct {
			sstate_t old; 
			sstate_t now;
		} packed change;

		struct {
			u16 code;
		} packed chkcode;

		struct {
			u16 old_code;
			u16 new_code;
		} packed newcode;
	} content;
} packed packet_t;

// actual data that's send, including a packet + framing bytes
typedef union {
	// structure of data
	struct {
#define PREAMBLE  (u32)(0b11001100110010101010110100110110)
#define POSTAMBLE (u32)(0b01100011001101010101001100110011)
		u32 pre; // packet frame preamble
		packet_t packet; // packet data
		u32 post; // packet frame postamble
	} packed s;

	// access to data (GNU extension and will cause warnings)
	u8 data[0];
} realpacket_t;

// serial event data
typedef struct {
#define RX   (1 << 0) // received packet
#define TX   (1 << 1) // transmitted packet
#define OK   (1 << 2) // operation successful
#define FAIL (1 << 3) // operation failed
#define FULL (1 << 4) // buffer is full
#define PRTY (1 << 5) // parity error
#define ORUN (1 << 6) // buffer overrun
#define FRAM (1 << 7) // framing error
	u8 flags;
	packet_t target; // not zeroed on error to save time
} sev_t;

// asynchronously transmit packet
u8 serial_tx(packet_t *packet, u8 flags);

// allow next transmitted packet to generate event
void serial_tx_next();

// allow next received packet to generate event
void serial_rx_next();

#endif // !SERIAL_H
