#include "globals.h"
#include "util/init.h"
#include "util/interrupt.h"
#include "common/serial.h"
#include "program/screen.h"
#include "program/button.h" 

// different program states
static sstate_t sstate = INIT;
static istate_t istate = BOOT;

// event data for each type
static stev_t stev_sstate = { .type = SHARED   };
static stev_t stev_istate = { .type = INTERNAL };

// how long to keep messages on screen
#define MSG_TICKS 10

// flags for the main program
#define HAVELINK (1 << 0) // link status
#define SSRECALL (1 << 1) // state change during boot
static u8 flags;

// state change dispatcher
#define change(what, to) ({ \
	stev_##what##state.old = what##state; \
	stev_##what##state.now = to; \
	dispatch(STATE, &stev_##what##state); })

// how long until we assume the serial link is broken
#define SERIAL_TIMEOUT_TICKS 5

// how often to send sync (detectes link loss)
#define SYNC_INTERVAL_TICKS 20

// serial transmission helper with timeout
#define tx(packet, flags) ({ \
	ev_set_id(SERIAL_TIMEOUT, 0); \
	serial_tx(packet, flags); \
	})

// tick counters for different timers
static u8 ticks; // changes between users
static u8 tout_ticks = SERIAL_TIMEOUT_TICKS;
static u8 sync_ticks; // should start at zero

// code input state
static u16 code_input;
static u8  code_index;
static u8  code_stage;

#define FRAME_TICKS 0
#define TITLE_TICKS 20

// cool boot animation, because why not
static const chr boot_text_1[SCREEN_COLS] PROGMEM = "  ALARM SYSTEM  ";
static const chr boot_text_2[SCREEN_COLS] PROGMEM = "    J.OVASKA    ";

static u8 boot_counter;

static u8 boot_sequence()
{
	// init
	if (unlikely(boot_counter == 0)) {
		screen_clear();
		ev_set_id(PROGRAM_TIMER, 0);
	}

	// reveal
	if (boot_counter < SCREEN_COLS) {
		// line 1
		if (boot_counter > 0) {
			screen_goto(0, boot_counter - 1);
			screen_putc(rom(boot_text_1[boot_counter - 1], byte), 0);
		} else {
			screen_goto(0, boot_counter);
		}
		screen_putc('#', 0);

		// line 2
		screen_goto(1, SCREEN_COLS - 1 - boot_counter);
		screen_putc('#', 0);
		if (boot_counter > 0)
			screen_putc(rom(boot_text_2[SCREEN_COLS - boot_counter], byte), 0);

		ticks = FRAME_TICKS;
	
	// keep text
	} else if (boot_counter == SCREEN_COLS) {
		// line 1
		screen_goto(0, 15);
		screen_putc(' ', 0);

		// line 2
		screen_goto(1, 0);
		screen_putc(' ', 0);

		ticks = TITLE_TICKS;
	
	// hide
	} else if (boot_counter < 2*SCREEN_COLS) {
		// line 1
		screen_goto(0, 2*SCREEN_COLS - boot_counter - 1);
		screen_putc('#', 0);
		if (boot_counter > (SCREEN_COLS + 1))
			screen_putc(' ', 0);

		// line 2
		if (boot_counter > (SCREEN_COLS + 1)) {
			screen_goto(1, boot_counter - SCREEN_COLS - 1);
			screen_putc(' ', 0);
		} else {
			screen_goto(1, boot_counter - SCREEN_COLS);
		}
		screen_putc('#', 0);

		ticks = FRAME_TICKS;
	
	// end of animation
	} else {
		// line 1
		screen_goto(0, 0);
		screen_putc(' ', 0);

		// line 2
		screen_goto(1, 15);
		screen_putc(' ', 0);

		screen_flush();

		boot_counter = 0;
		return 1;
	}

	// commit to display
	screen_flush();

	boot_counter++;
	return 0;
}

#define ANIM_TICKS 10

static const chr idle_text[] PROGMEM = "PRESS ANY KEY";

// __builtin_strlen() will hopefully eval at compile time 
#define MOVEMENT (SCREEN_COLS - __builtin_strlen(idle_text))

static u8 anim_counter;

static void idle_animation()
{
	if (anim_counter < 1) {
		screen_goto(1, 0);
		screen_puts((ptr)idle_text, NULLTERM, ROMSTR);
		screen_putc(' ', 0);
	} else if (anim_counter <= MOVEMENT) {
		screen_goto(1, anim_counter - 1);
		screen_putc(' ', 0);
		screen_puts((ptr)idle_text, NULLTERM, ROMSTR);
	} else {
		screen_goto(1, 2*MOVEMENT - anim_counter);
		screen_puts((ptr)idle_text, NULLTERM, ROMSTR);
		screen_putc(' ', 0);
	}

	screen_flush();
	anim_counter = (anim_counter + 1) % (2*MOVEMENT);  
}

#define DEF_PSTR_PTR(name, str) \
	static const char PSTR_PTR_##name[] PROGMEM = (str)

#define REF_PSTR_PTR(name) \
	(ptr)&(PSTR_PTR_##name)

DEF_PSTR_PTR(INIT, "  WAIT");
DEF_PSTR_PTR(ARMD, " ARMED");
DEF_PSTR_PTR(ALRT, " ALERT");
DEF_PSTR_PTR(ALRM, " ALARM");
DEF_PSTR_PTR(ULCK, "UNLOCK");

DEF_PSTR_PTR(BOOT, "BOOT"); // unused
DEF_PSTR_PTR(LINK, "LINK"); // unused
DEF_PSTR_PTR(CODE, "CODE");
DEF_PSTR_PTR(MENU, "MENU");
DEF_PSTR_PTR(IDLE, "IDLE");

static const str sstate_str[] PROGMEM = {
	REF_PSTR_PTR(INIT),
	REF_PSTR_PTR(ARMD),
	REF_PSTR_PTR(ALRT),
	REF_PSTR_PTR(ALRM),
	REF_PSTR_PTR(ULCK)
};

static const str istate_str[] PROGMEM = {
	REF_PSTR_PTR(BOOT),
	REF_PSTR_PTR(LINK),
	REF_PSTR_PTR(CODE),
	REF_PSTR_PTR(MENU),
	REF_PSTR_PTR(IDLE)
};

// first line is used for state display
static void update_stdisp()
{
	ptr p;
	u8 n;

	// internal state (left)
	screen_goto(0, 0);
	screen_putc(0x7F, 0);
	screen_puts(pgm_read_ptr(&istate_str[istate]), NULLTERM, ROMSTR);

	p = pgm_read_ptr(&sstate_str[sstate]);
	n = strlen_P(p);

	// shared state (right)
	screen_goto(0, SCREEN_COLS - 1 - n);
	screen_puts(p, NULLTERM, ROMSTR);
	screen_putc(0x7E, 0);

	screen_flush();
}

DEF_PSTR_PTR(RERM, "RE-ARM");
DEF_PSTR_PTR(CCDE, "CHCODE");

static const str menu_str[] PROGMEM = {
	REF_PSTR_PTR(RERM),
	REF_PSTR_PTR(CCDE)
};

static u8 menu_item;

static void menu_next()
{
	//menu_item = (menu_item + 1) % length(menu_str);
	menu_item = (menu_item + 1) & 1;

	screen_goto(1, 5);
	screen_puts(pgm_read_ptr(&menu_str[menu_item]), NULLTERM, ROMSTR);
	screen_flush();
}

static void sstate_change(sstate_t old, sstate_t now)
{
	save_int();

	// recall after boot
	if (unlikely(istate == BOOT)) {
		flags |= SSRECALL;
		return; 
	}

	// race condition with no link
	if (unlikely(istate == LINK))
		return;

	// disable backlight blink
	if (now != ALRM)
		screen_backlight(ON);

	update_stdisp();

	// old state cleanup
	switch (old) {
	case INIT: // uninitialized
		break;

	case ARMD: // waiting for motion
		break;

	case ALRT: // motion detected
		break;

	case ALRM: // alarm on
		break;

	case ULCK: // unlocked, alarm off
		change(i, IDLE);
		break;
	}

	// new state init
	switch (now) {
	case INIT: // uninitialized
		break;

	case ARMD: // waiting for motion
		break;

	case ALRT: // motion detected
		break;

	case ALRM: // alarm on
		if (istate == IDLE)
			screen_backlight(BLINK);
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
	// old state cleanup
	switch (old) {
	case BOOT: // booting up
		if (flags & SSRECALL)
			change(s, sstate);
		break;

	case LINK: // waiting for link
		screen_clear();
		break;

	case CODE: // code prompt
		screen_goto(1, 0);
		screen_cursor(OLDPOS, OLDPOS, NONE);
		screen_puts(PSTR("                "), NULLTERM, ROMSTR);
		screen_flush();

		// reset state
		code_input = code_index = code_stage = 0;
		break;

	case MENU: // main menu
		screen_goto(1, 0);
		screen_puts(PSTR("                "), NULLTERM, ROMSTR);
		screen_flush();
		break;

	case IDLE: // idle state
		ev_set_id(PROGRAM_TIMER, 1);
		break;
	}

	if ((istate != BOOT) && (istate != LINK))
		update_stdisp();
	
	// backlight handling
	if (sstate == ALRM) {
		if (istate == IDLE)
			screen_backlight(BLINK);
		else
			screen_backlight(ON);
	}

	// new state init
	switch (now) {
	case BOOT: // booting up
		(void) boot_sequence();
		break;

	case LINK: // waiting for link
		screen_clear();
		screen_goto(0, 7);
		screen_puts(PSTR("NO"), NULLTERM, ROMSTR);
		screen_goto(1, 6);
		screen_puts(PSTR("LINK"), NULLTERM, ROMSTR);
		screen_flush();
		break;

	case CODE: // code prompt
		screen_goto(1, 0);
		screen_puts(PSTR("   * [    ] #   "), NULLTERM, ROMSTR);
		screen_cursor(1, 6, BOTH);
		screen_flush();
		break;

	case MENU: // main menu
		screen_goto(1, 0);
		screen_puts(PSTR("  * >      < #  "), NULLTERM, ROMSTR);
		//menu_item = length(menu_str) - 1;
		menu_item = 1;
		menu_next();
		break;

	case IDLE: // idle state
		anim_counter = 0;
		ticks = ANIM_TICKS;
		idle_animation();
		ev_set_id(PROGRAM_TIMER, 0);
		ev_set_id(BUTTON_INPUT, 0);
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

// last transmitted packet
static packet_t *last_tx;

// serial event handler
u8 e_serial_packet(u8 unused id, u8 unused code, sev_t *arg)
{
	// no fail condition at the moment, silently ignored
	if (arg->flags & FAIL)
		return 0;

	// transmitted packet
	if (arg->flags & TX) {
		// messages are transmitted without timeout
		if (arg->target.mode == REQUEST)
			last_tx = &arg->target; // kept at least until serial_tx_next()

		// no need to keep this packet
		else
			serial_tx_next();

	// has to be a received packet
	} else {
		// reset & stop timeout timer
		tout_ticks = SERIAL_TIMEOUT_TICKS;
		ev_set_id(SERIAL_TIMEOUT, 1);

		// set link flag
		flags |= HAVELINK;

		// init state change
		if (istate == LINK)
			change(i, IDLE);

		// handle packets
		switch (arg->target.type) {
		// manual state sync
		case SYNC:
			// initiate state change
			if (sstate != arg->target.content.sync.now)
				change(s, arg->target.content.sync.now);
			break;

		// state change
		case CHANGE:
			// message from backend
			if (arg->target.mode == MESSAGE)
				change(s, arg->target.content.change.now);
			break;

		// check code or new code
		case CHKCODE:
		case NEWCODE:
			// give feedback
			screen_goto(1, 6);
			if (arg->target.header.response.status == OK)
				screen_puts(PSTR(" OK "), NULLTERM, ROMSTR);
			else
				screen_puts(PSTR("FAIL"), NULLTERM, ROMSTR);
			screen_flush();

			// timeout to idle mode
			ticks = MSG_TICKS;
			ev_set_id(PROGRAM_TIMER, 0);
			break;
		}

		// response packet clears this
		if (arg->target.mode == RESPONSE) {
			last_tx = NULL;
			serial_tx_next();
		}

		serial_rx_next(); // allow next packet to be received
	}

	return 0;
}

static packet_t my_packet;

// code input handler
u8 e_oncode_input(u8 unused id, u8 unused code, u16 *arg)
{
	// which code are we inputting
	if (sstate == ULCK) {
		if (code_stage < 1) {
			// prepare part of packet
			my_packet.type = NEWCODE;
			my_packet.mode = REQUEST;
			my_packet.content.newcode.old_code = *arg;

			// update screen
			screen_goto(1, 6);
			screen_puts(PSTR("    "), NULLTERM, ROMSTR);
			screen_goto(1, 6);
			screen_flush();
			screen_cursor(1, 6, BOTH);

			// change button code state
			code_stage++;
			code_input = code_index = 0;
			ev_set_id(BUTTON_INPUT, 0);
		} else {
			my_packet.content.newcode.new_code = *arg;
			tx(&my_packet, 0);
		}
	} else {
		// unlock code
		my_packet.type = CHKCODE;
		my_packet.mode = REQUEST;
		my_packet.content.chkcode.code = *arg;
		tx(&my_packet, 0);
	}

	return 0;
}

// menu selection handler
u8 e_menu_selection(u8 unused id, u8 unused code, u8 *arg)
{
	switch (*arg) {
	// re-arm
	case 0:
		// transmit state change packet
		my_packet.type = CHANGE;
		my_packet.mode = REQUEST;
		my_packet.content.change.now = ARMD;
		tx(&my_packet, 0);
		break;
	
	// change code
	case 1:
		// code state automatically handles this
		change(i, CODE);
		ev_set_id(BUTTON_INPUT, 0);
		break;
	}
	return 0;
}

// button event handler
u8 e_button_input(u8 unused id, u8 unused code, u32 *arg)
{
	// only care about UP events
	if (!(*arg & UP))
		return 0;

	// button actions depend on UI state
	switch (istate) {
	case IDLE:
		// if backend is unlocked, go into menu
		if (sstate == ULCK)
			change(i, MENU);
		else
			change(i, CODE);
		break;

	case CODE:
		// * is backspace
		if (*arg & KS) {
			// last character --> back to idle mode
			if (code_index < 1) {
				code_stage = 0;
				change(i, IDLE);
				break;
			}

			code_index--;

			// erase character
			screen_goto(1, 6 + code_index);
			screen_putc(' ', 0);
			screen_flush();

			// erase from code_input
			code_input &= ~(0xF << 2*code_index);

			// move cursor
			if (code_index < 3)
				screen_cursor(1, 6 + code_index, OLD);

			// show cursor
			else
				screen_cursor(OLDPOS, OLDPOS, BOTH);

		// # is enter
		} else if (*arg & KH) {
			// partial code_input
			if (code_index < 4)
				break;

			// dispatch event
			dispatch(ONCODE, &code_input);

			// disable self
			ev_set_id(id, 1);

		// code_input input
		} else {
			// next char would overflow
			if (code_index >= 4)
				break;

			// put character
			screen_goto(1, 6 + code_index);
			screen_putc(ktoc(*arg), 0);
			screen_flush();

			// add to code_input
			code_input |= kton(*arg) << 2*code_index;

			code_index++;

			// move cursor
			if (code_index < 4)
				screen_cursor(1, 6 + code_index, OLD);

			// hide cursor
			else
				screen_cursor(OLDPOS, OLDPOS, NONE);
		}
		break;

	case MENU:
		// * is backspace
		if (*arg & KS) {
			change(i, IDLE);
		
		// # is enter
		} else if (*arg & KH) {
			dispatch(SELECT, &menu_item);
			ev_set_id(id, 1); // disable self

		// other keys cycle menu
		} else {
			menu_next();
		}

		break;

	default:
		ev_set_id(id, 1); // disable self
		break;
	}

	return 0;
}

// timer event handler
u8 e_program_timer(u8 unused id, u8 unused code, ptr unused arg)
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
			// we have a link
			if (flags & HAVELINK)
				change(i, IDLE);

			// otherwise go to no link state
			else
				change(i, LINK);

			// disable self
			ev_set_id(id, 1);
		}
		break;
	
	// switch to idle mode and activate buttons
	case CODE:
		ev_set_id(BUTTON_INPUT, 0);
		change(i, IDLE);
		break;
	
	// idle animation
	case IDLE:
		idle_animation();
		ticks = ANIM_TICKS;
		break;
	
	// not configured
	default:
		ev_set_id(id, 1); // stop
		break;
	}

	return 0;
}

// serial timeout (separate from program timer)
u8 e_serial_timeout(u8 id, u8 unused code, ptr unused arg)
{
	// wait
	if (tout_ticks-- > 0)
		return 0;
	tout_ticks = SERIAL_TIMEOUT_TICKS;

	// switch to no link state
	if ((istate != BOOT) && (istate != LINK))
		change(i, LINK);

	// clear link flag
	flags &= ~HAVELINK;

	// clear last transmitted
	last_tx = NULL;

	// allow next packet to be transmitted
	serial_tx_next();

	ev_set_id(id, 1); // disable self

	return 0;
}

// this remains static
static const packet_t sync_packet PROGMEM = { .type = SYNC, .mode = REQUEST };

// state synchronizer
u8 e_stsync_timer(u8 unused id, u8 unused code, ptr unused arg)
{
	// wait
	if (sync_ticks-- > 0)
		return 0;
	sync_ticks = SYNC_INTERVAL_TICKS;

	// transmit
	tx((ptr)&sync_packet, ROMDATA);

	return 0;
}

// initial event (can't use dispatch() as .init section code
// stack isn't addressable by functions for some reason)
static const event_t boot_event PROGMEM = { STATE, &stev_istate };
INIT() { (void)event_dispatch(&g_event_loop, &boot_event, ROMDATA); }
