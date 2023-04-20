#include "util/init.h"
#include "common/serial.h"
#include "program/alarm.h"
#include "program/motion.h"

#include <avr/io.h>

// serial event handler
u8 e_serial_packet(u8 unused id, u8 unused code, sev_t *arg)
{
	if (arg->flags & RX) {
		packet_t packet;
		packet.type = ACK;
		packet.content.ack.status = OK;
		serial_tx(&packet, 0);
		serial_rx_next();
	} else if (arg->flags & TX) {
		serial_tx_next();
	}

	return 0;
}

u8 ticks = 10;

u8 e_send_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	if (ticks-- > 0)
		return 0;
	ticks = 10;

	packet_t packet;
	packet.type = CHKLINK;
	serial_tx(&packet, 0);

	return 0;
}

// motion event handler
u8 e_motion_trigger(u8 unused id, u8 unused code, ptr unused arg)
{
	alarm_set(0);
	return 0;
}

INIT()
{
	// allow serial events
	serial_rx_next();
	serial_tx_next();

	// enable motion detector
	motion_set(0);

	// set alarm to 500Hz
	alarm_freq(500);
}
