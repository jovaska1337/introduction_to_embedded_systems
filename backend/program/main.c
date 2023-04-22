#include "globals.h"
#include "util/init.h"
#include "util/interrupt.h"
#include "common/serial.h"
#include "program/alarm.h"
#include "program/motion.h"

#include <avr/eeprom.h>

#define ALARM_TIMEOUT_TICKS 100

// different program states
static sstate_t sstate = INIT;
static istate_t istate = BOOT;

// event data for each type
static stev_t stev_sstate = { .type = SHARED   };
static stev_t stev_istate = { .type = INTERNAL };

// state change dispatcher
#define change(what, to) ({ \
	stev_##what##state.old = what##state; \
	stev_##what##state.now = to; \
	dispatch(STATE, &stev_##what##state); })

// timer ticks
static u8 ticks;

static u8 boot_counter;

static u8 boot_sequence()
{
	// boot sequence over (over 10 becomes quiet at end,
	// frequency parameter selection algorithm is probably bad)
	if (unlikely(boot_counter >= 10)) {
		buzzer_set(0, BUZOFF);
		boot_counter = 0;
		return 1;
	}
	ticks = 0;

	// frequency sweep
	buzzer_set(20*boot_counter + 300, BUZON);

	// init
	if (unlikely(boot_counter == 0))
		ev_set_id(PROGRAM_TIMER, 0);

	boot_counter++;
	return 0;
}

// TODO: put in internal EEPROM
static u16 code_mem EEMEM;
static u16 code_ram;

static u8 check_code(u16 code)
{
	// check against RAM code for speed
	return code_ram == code;
}

static u8 change_code(u16 old, u16 new)
{
	if (old != code_ram)
		return 0;

	// update code in RAM and EEPROM
	code_ram = new;
	eeprom_update_word(&code_mem, new);

	return 1;
}

static void sstate_change(sstate_t old, sstate_t now)
{
	save_int();

	switch (now) {
	case INIT: // uninitialized
		change(s, ARMD);
		break;

	case ARMD: // waiting for motion
		motion_set(0); // enable
		break;

	case ALRT: // motion detected
		// begin alarm timeout
		ticks = ALARM_TIMEOUT_TICKS;
		ev_set_id(PROGRAM_TIMER, 0);
		break;

	case ALRM: // alarm on
		alarm_set(0); // enable alarm
		break;

	case ULCK: // unlocked, alarm off
		switch (old) {
		// disable motion detection
		case ARMD:
			motion_set(1);
			break;

		// stop timer
		case ALRT:
			ev_set_id(PROGRAM_TIMER, 0);
			break;

		// stop alarm
		case ALRM:
			alarm_set(1);
			break;

		default:
			break;
		}
		break;
	}

	rest_int();
}

static void istate_change(istate_t unused old, istate_t now)
{
	save_int();

	switch (now) {
	case BOOT: // booting up
		(void) boot_sequence();
		break;

	case IDLE: // waiting for events
		// allow serial events
		serial_rx_next();
		serial_tx_next();

		// change shared state to init
		change(s, INIT);
		break;
	}

	rest_int();
}

// state change event handler
u8 e_state_change(u8 unused id, u8 unused code, stev_t *data)
{
	switch (data->type) {
	// shared state changes (synced by us)
	case SHARED:
		// prepare sync packet
		packet_t tmp;
		tmp.type = CHANGE;
		tmp.mode = MESSAGE;
		tmp.content.change.old = data->old;
		tmp.content.change.now = data->now;

		// transmit sync packet
		serial_tx(&tmp, 0);

		// internal change handler
		sstate = data->now;
		sstate_change(data->old, data->now);
		break;

	// internal state changes
	case INTERNAL:
		istate = data->now;
		istate_change(data->old, data->now);
		break;
	}

	return 0;
}

// serial event handler
u8 e_serial_packet(u8 unused id, u8 unused code, sev_t *arg)
{
	if (arg->flags & FAIL)
		return 0;

	// transmitted packet
	if (arg->flags & TX) {
		serial_tx_next(); // allow next packet to transmit

	// has to be a received packet
	} else {
		packet_t tmp;

		// handle packets
		switch (arg->target.type) {
		// manual state sync
		case SYNC:
			// prepare response
			tmp.type = SYNC;
			tmp.mode = RESPONSE;
			tmp.header.response.status = OK;
			tmp.content.sync.now = sstate;

			// transmit response
			serial_tx(&tmp, 0);
			break;

		// state change
		case CHANGE:
			tmp.type = CHANGE;
			tmp.mode = RESPONSE;

			// frontend can initiate state change when unlocked
			if (sstate == ULCK) {
				tmp.header.response.status = OK;
			
				// dispatch state change and let
				// the handler sync shared state
				change(s, arg->target.content.change.now);

			} else {
				tmp.header.response.status = FAIL;
			}
			
			// transmit response
			serial_tx(&tmp, 0);
			break;

		// check code (initiates state change on success)
		case CHKCODE:
			tmp.type = CHKCODE;
			tmp.mode = RESPONSE;

			// check code
			if (check_code(arg->target.content.chkcode.code)) {
				tmp.header.response.status = OK;

				// change state to unlocked
				change(s, ULCK);

			} else {
				tmp.header.response.status = FAIL;
			}

			// transmit response
			serial_tx(&tmp, 0);
			break;

		// change code
		case NEWCODE:
			tmp.type = NEWCODE;
			tmp.mode = RESPONSE;

			// code can only be changed when unlocked
			if (sstate == ULCK) {
				// try to change
				if (change_code(
					arg->target.content.newcode.old_code,
					arg->target.content.newcode.new_code)
				) {
					tmp.header.response.status = OK;

				// invalid old code
				} else {
					tmp.header.response.status = FAIL;
				}
			
			// acknowledge with error
			} else {
				tmp.header.response.status = FAIL;
			}

			// transmit response
			serial_tx(&tmp, 0);
			break;
		}

		serial_rx_next(); // allow next packet to be received
	}

	return 0;
}

// motion event handler
u8 e_motion_trigger(u8 unused id, u8 unused code, ptr unused arg)
{
	switch (sstate) {
	// trigger alerted state
	case ARMD:
		// dispatch state change
		change(s, ALRT);
		fallthrough;

	// disable motion detection
	default:
		motion_set(1);
		break;
	}

	return 0;
}

// timer event handler
u8 e_program_timer(u8 id, u8 unused code, ptr unused arg)
{
	// wait
	if (ticks-- > 0)
		return 0;

	// internal state overrides shared state
	switch (istate) {
	// boot state (turning on)
	case BOOT:
		// switch to on
		if (boot_sequence()) {
			// change state
			change(i, IDLE);

			// disable self
			ev_set_id(id, 1);
		}
		break;

	// waiting state (on)
	case IDLE:
		switch (sstate) {
		// trigger alarm on timeout
		case ALRT:
			// dispatch state change
			change(s, ALRM);

			ev_set_id(id, 1); // stop
			break;
		
		// not configured
		default:
			ev_set_id(id, 1); // stop
			break;
		}
	}

	return 0;
}

// initial event (can't use dispatch() as .init section code
// stack isn't addressable by functions for some reason)
static const event_t boot_event PROGMEM = { STATE, &stev_istate };
INIT()
{
#if 0
	// initial EEPROM code
	eeprom_update_word(&code_mem, 0);
	while (1);
#endif
	// read code into ram
	code_ram = eeprom_read_word(&code_mem);

	// dispatch boot event
	(void)event_dispatch(&g_event_loop, &boot_event, ROMDATA);
}
