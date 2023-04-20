#include "globals.h"
#include "util/init.h"
#include "util/memory.h"
#include "util/interrupt.h"
#include "common/defs.h"
#include "common/serial.h" 

// FIXME: this is slightly incomplete but it works as is

#include <string.h>

// select USART registers (operations are same for ATmega328P and ATmega2560)
#if PLATFORM == MEGA

// interrupt vectors
#define USART_RX_vect USART1_RX_vect
#define USART_TX_vect USART1_TX_vect
#define USART_UDRE_vect USART1_UDRE_vect

// registers
#define UDR   UDR1
#define UCSRA UCSR1A
#define UCSRB UCSR1B
#define UCSRC UCSR1C
#define UBRRL UBRR1L
#define UBRRH UBRR1H

// if there is a God, the bits in these registers are the same, regardless
// which USART port is selected (which means we don't redefine them here)

#elif PLATFORM == UNO

// interrupt vectors for ATmega328P already have
// the names we define for ATmega2560 above

// registers
#define UDR   UDR0
#define UCSRA UCSR0A
#define UCSRB UCSR0B
#define UCSRC UCSR0C
#define UBRRL UBRR0L
#define UBRRH UBRR0H

#endif

// these have to be separate due to flexible members
// (compiler braindamage, would work fine in theory)
static volatile ring_t rx_buf = ring_init(packet_t, SERIAL_BUFSIZE);
static volatile ring_t tx_buf = ring_init(packet_t, SERIAL_BUFSIZE);

// internal state machine
static volatile struct {

#define RX_DISPATCH (1 << 0) // received packet can dispatch event
#define TX_DISPATCH (1 << 1) // transmitted packet can dispatch event
#define TX_PROGRESS (1 << 2) // transmission in progress
#define TX_BLOCKING (1 << 3) // transmission blocking

	u8 flags; // state machine flags

	// byte positions for interrupt handlers
	u8 rx_byte;
	u8 tx_byte;

	// pointers to where bytes are being copied from/to
	realpacket_t rx;
	realpacket_t tx;

	// we can safely allocate these here
	sev_t rx_ev;
	sev_t tx_ev;

} packed state = {
	.flags   = 0,
	.rx_byte = 0,
	.tx_byte = 0,
	.rx      = { .s = {PREAMBLE, {}, POSTAMBLE} },  // rx packet data
	.tx      = { .s = {PREAMBLE, {}, POSTAMBLE} },  // tx packet data
	.rx_ev   = {}, // rx event data
	.tx_ev   = {}, // tx event data
};

// I know where these happen and I don't need constant reminders
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wzero-length-bounds"
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"

u8 serial_tx(packet_t *packet, u8 flags)
{
	int ret = 1; // fail

	save_int();

	// operation in progress
	if (state.flags & TX_PROGRESS) {
		// add to buffer
		ret = ring_put(&tx_buf, packet, flags) == NULL;
		goto end;

	// begin new operation (no need to involve buffer)
	} else {
		// copy data to transmit buffer
		copy(&state.tx.s.packet, packet, sizeof(*packet), flags);

		// set transmit state
		state.flags  |= TX_PROGRESS;
		state.tx_byte = 0;

		// begin transmission
		UDR = state.tx.data[state.tx_byte++];
		UCSRB |= _BV(UDRIE0); // enable ISR
	}

	ret = 0; // success
end:
	rest_int();

	return ret;
}

void serial_tx_next()
{
	save_int();

	// blocking on transmitted packet
	if (state.flags & TX_BLOCKING) {
		// dispatch event (prepared in ISR)
		dispatch(SERIAL, (ptr)&state.tx_ev);

		// clear blocking flag
		state.flags &= ~TX_BLOCKING;

		// any packets left?
		if (ring_pop(&tx_buf, &state.tx.s.packet) == NULL) {
			state.flags &= ~TX_PROGRESS;
			goto end;
		}

		// set transmit state
		state.tx_byte = 0;

		// begin transmission
		UDR = state.tx.data[state.tx_byte++];
		UCSRB |= _BV(UDRIE0); // enable ISR

	} else { 
		// set dispatch flag
		state.flags |= TX_DISPATCH;
	}
end:
	rest_int();
}

void serial_rx_next()
{
	save_int();

	// buffered packets
	if (ring_pop(&rx_buf, &state.rx_ev.target) != NULL) {
		// dispatch immediately
		state.rx_ev.flags = RX | OK;
		dispatch(SERIAL, (ptr)&state.rx_ev);

	// no packets in buffer
	} else {
		state.flags |= RX_DISPATCH;
	}

	rest_int();
}


// RX complete interrupt (receive byte)
ISR(USART_RX_vect)
{
	// current packet has unreceived bytes
	if (likely(state.rx_byte < sizeof(state.rx.s))) {
		// append received byte
		state.rx.data[state.rx_byte++] = UDR;	

		// preamble mismatch
		if (unlikely(
			(state.rx_byte == sizeof(state.rx.s.pre))
			&& (state.rx.s.pre != PREAMBLE))
		)
			// discard first byte
			(void) memmove((ptr)state.rx.data,
				(ptr)&state.rx.data[1], --state.rx_byte);
	}

	// current packet is done
	if (unlikely(state.rx_byte >= sizeof(state.rx.s))) {
		// postamble mismatch
		if (unlikely(state.rx.s.post != POSTAMBLE)) {
			// dispatch error
			state.rx_ev.flags = RX | FAIL | FRAM;
			dispatch(SERIAL, (ptr)&state.rx_ev);
		} else {
			// put packet in buffer
			if (ring_put(&rx_buf, &state.rx.s.packet, 0) == NULL) {
				// buffer is full
				state.rx_ev.flags = RX | FAIL | FULL;
				dispatch(SERIAL, (ptr)&state.rx_ev);
				goto prep;
			}

			// dispatch next packet if possible
			if (state.flags & RX_DISPATCH) {
				// dispatch event
				(void) ring_pop(&rx_buf, &state.rx_ev.target);
				state.rx_ev.flags = RX | OK;
				dispatch(SERIAL, (ptr)&state.rx_ev);

				// clear can dispatch flag
				state.flags &= ~RX_DISPATCH;
			}
		}
prep:
		// prepare to receive next packet
		state.rx_byte = 0;
	}
}

// USART data register empty
ISR(USART_UDRE_vect)
{
	// current packet has untransmitted bytes
	if (likely(state.tx_byte < sizeof(state.tx.s)))
		UDR = state.tx.data[state.tx_byte++]; // transmit next

	// transmission complete
	if (unlikely(state.tx_byte >= sizeof(state.tx.s))) {
		// prepare event
		(void) memcpy(&state.tx_ev.target, 
			&state.tx.s.packet, sizeof(state.tx_ev.target));
		state.tx_ev.flags = TX | OK;

		// can't continue if last dispatch hasn't been handled
		if (!(state.flags & TX_DISPATCH)) {
			state.flags |= TX_BLOCKING;
			goto done;
		}

		// dispatch for sent packet
		dispatch(SERIAL, (ptr)&state.tx_ev);
		
		// clear dispatch flag
		state.flags &= ~TX_DISPATCH;

		// no queued packets
		if (ring_pop(&tx_buf, &state.tx.s.packet) == NULL) {
			state.flags &= ~TX_PROGRESS;
			goto done;
		}

		// set transmit state
		state.tx_byte = 0;

		// begin transmission
		UDR = state.tx.data[state.tx_byte++];
	}

	// can transmit more
	return;

	// nothing to transmit
done:
	UCSRB &= ~_BV(UDRIE0); // disable ISR
}

INIT()
{
	// baud setup (16MHz clk -> 250kbs)
	UBRRH = 0;
	UBRRL = 3;

	// enable double speed (250kbs -> 500kbs)
	UCSRA = _BV(U2X0);

	// enable interrupts + RX/TX
	UCSRB = _BV(RXCIE0) | _BV(RXEN0) | _BV(TXEN0);

	// asynchronous receiver + even parity + 1 bit stop + 8 bit char
	UCSRC = _BV(UPM01) | _BV(UCSZ01) | _BV(UCSZ00);
}

#pragma GCC diagnostic pop
