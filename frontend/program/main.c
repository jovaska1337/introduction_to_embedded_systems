#include "util/init.h"
#include "util/attr.h"
#include "util/type.h"
#include "common/wait.h"
#include "common/serial.h"
#include "program/screen.h"
#include "program/button.h" 

#include <avr/pgmspace.h>

static u8 n_rx, n_tx, n_rxf, n_txf;

// serial event handler
u8 e_serial_packet(u8 unused id, u8 unused code, sev_t *arg)
{
	if (arg->flags & TX) {
		if (arg->flags & OK) {
			n_tx++;
			screen_goto(0, 3);
			screen_puti(n_tx, 10, I8 | SPAD);
		} else {
			n_txf++;
			screen_goto(0, SCREEN_COLS - 3);
			screen_puti(n_txf, 10, I8 | SPAD);
		}

		screen_flush();
		serial_tx_next();

	} else if (arg->flags & RX) {
		if (arg->flags & OK) {
			n_rx++;
			screen_goto(1, 3);
			screen_puti(n_rx, 10, I8 | SPAD);
		} else {
			n_rxf++;
			screen_goto(1, SCREEN_COLS - 3);
			screen_puti(n_rxf, 10, I8 | SPAD);
		}

		screen_flush();
		serial_rx_next();
	}

	return 0;
}

// button event handler
u8 e_button_input(u8 unused id, u8 unused code, ptr arg)
{
	switch (*(u32 *)arg) {
	case KH|UP:
		packet_t packet;
		packet.type = CHKLINK;
		for (u8 i = 0; i < 8; i++)
		{
			serial_tx(&packet, 0);
			wait_ms(1);
		}
		break;
	case KS|UP:
		serial_tx_next();
		break;
	case K0|UP:
		serial_rx_next();
		break;
	case K1|UP:
		break;
	case K2|UP:
		break;
	}

	return 0;
}

INIT()
{
	// allow serial events
	serial_rx_next();
	serial_tx_next();
}

INIT(8)
{
	screen_goto(0, 0);
	screen_puts(PSTR("TX:"), NULLTERM, ROMSTR);
	screen_goto(1, 0);
	screen_puts(PSTR("RX:"), NULLTERM, ROMSTR);
	screen_flush();
}
