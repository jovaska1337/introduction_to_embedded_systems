#include "globals.h"
#include "util/init.h"
#include "util/interrupt.h"
#include "common/serial.h"
#include "program/screen.h"
#include "program/button.h" 

// different program states
static sstate_t  sstate  = INIT;
static istate_t  istate  = BOOT;
static uistate_t uistate = ANIM;

// event data for each type
static stev_t stev_sstate  = { .type = SHARED   };
static stev_t stev_istate  = { .type = INTERNAL };
static stev_t stev_uistate = { .type = UI       };

// state change dispatcher
#define change(what, to) ({ \
	stev_##what##state.old = what##state; \
	stev_##what##state.now = to; \
	dispatch(STATE, &stev_##what##state); })

static void sstate_change(sstate_t old, sstate_t now)
{
	save_int();

	switch (now) {
	case INIT: // uninitialized
		break;

	case ARMD: // waiting for motion
		break;

	case ALRT: // motion detected
		break;

	case ALRM: // alarm on
		break;

	case ULCK: // unlocked, alarm off
		break;
	}

	rest_int();
}

static void istate_change(istate_t old, istate_t now)
{
	save_int();

	// transition from boot should enable serial
	if (unlikely(old == BOOT)) {
		// allow serial events
		serial_rx_next();
		serial_tx_next();
	}

	switch (now) {
	case BOOT: // booting up
		break;

	case LINK: // waiting for link
		break;

	case WAIT:  // waiting for input
		break;
	}

	rest_int();
}

static void uistate_change(uistate_t old, uistate_t now)
{
	save_int();

	switch (now) {
	case ANIM: // boot animation
		break;

	case NLNK: // no link
		break;

	case CODE: // code prompt
		break;

	case MENU: // main menu
		break;

	case IDLE:  // idle state
		break;
	}

	rest_int();
}

// state change event handler
u8 e_state_change(u8 unused id, u8 unused code, stev_t *data)
{
	switch (data->type) {
	// shared state changes (synced by backend)
	case SHARED:
		sstate_change(data->old, data->now);
		sstate = data->now;
		break;

	// internal state changes
	case INTERNAL:
		istate_change(data->old, data->now);
		istate = data->now;
		break;

	// ui state changes
	case UI:
		uistate_change(data->old, data->now);
		uistate = data->now;
		break;
	}

	return 0;
}

// serial event handler
u8 e_serial_packet(u8 unused id, u8 unused code, sev_t *arg)
{
	// no fail condition at the moment, silently ignored
	if (arg->flags & FAIL)
		return 0;

	// transmitted packet
	if (arg->flags & TX) {
		serial_tx_next();

	// has to be a received packet
	} else {
		// handle packets
		switch (arg->target.type) {
		// previous transmission acknowledged
		case ACK:
			// TODO: link timeout, check previous transmission
			break;

		// manual state sync (doesn't apply here)
		case SYNC:
			// prepare response
			packet_t tmp;
			tmp.type = ACK;
			tmp.content.ack.status = ERROR;

			// send response
			serial_tx(&tmp, 0);
			break;

		// state change
		case CHANGE:
			break;

		// check code
		case CHKCODE:
			break;

		// change code
		case NEWCODE:
			break;
		}
	
		serial_rx_next(); // allow next packet to be received
	}

	return 0;
}

// button event handler
u8 e_button_input(u8 unused id, u8 unused code, u32 *arg)
{
	return 0;
}

// timer event handler
u8 e_program_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	return 0;
}

// initial event (can't use dispatch() as .init section code
// stack isn't addressable by functions for some reason)
static const event_t boot_event PROGMEM = { STATE, &stev_istate };
INIT() { (void)event_dispatch(&g_event_loop, &boot_event, ROMDATA); }
