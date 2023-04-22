#include "globals.h"
#include "util/init.h"
#include "util/interrupt.h"
#include "common/wait.h"
#include "program/screen.h" 

#include <avr/cpufunc.h>
#include <avr/pgmspace.h>

#include <math.h>
#include <string.h>

// LOW LEVEL IO

void screen_io(sio_t type, u8 *data)
{
	save_int();

	// see HD44780 datasheet for more info here

	PORTL = (PORTL & ~3) | type; // set RS & R/W

	// wait for > 60 ns (~120 ns for 2)
	_NOP();
	_NOP();
	_NOP();

	PORTL |= _BV(2); // clock high

	// read
	if (type & 2) {
		DDRC = 0;

		// data setup takes max 360ns (~6 nops) from clock edge
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();

		*data = PINC;

	// write 
	} else {
		// this can be done immediately after clock edge
		PORTC = *data;

		// have to wait 80 ns (~2 nops) before clock edge
		_NOP();
		_NOP();
		_NOP();
	}	

	PORTL &= ~_BV(2); // clock low

	// read
	if (type & 2) {
		// wait for > 360 ns (~6 nops)
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();
		_NOP();
	}

	// reset pins
	DDRC   = ~0;
	PORTC  =  0;
	PORTL &= ~3;

	rest_int();
}

// wait for controller to clear busy flag
void screen_wait()
{
	u8 data;
loop:
	screen_io(RIR, &data);
	if (data & RIR_BF) {
		wait_us(50);
		goto loop;
	}
}

// HIGH LEVEL BUFFERED IO

// screen state
static struct {
	// screen buffer with current + flushed state
	chr buffer[2*SCREEN_ROWS][SCREEN_COLS];

	// current row and column
	u8 row;
	u8 col;
} packed state;

void screen_reset()
{
	u8 data;

	// perform 8-bit init (follows datasheet instruction)
	wait_ms(15);
	data = INS_FSET | FLG_FSET_MODE | FLG_FSET_LINE;
	screen_io(WIR, &data);
	wait_ms(5);
	screen_io(WIR, &data);
	wait_us(110);
	screen_io(WIR, &data);

	// set display mode (no cursors initially)
	data = INS_CTRL | FLG_CTRL_DISP;
	screen_wait();
	screen_io(WIR, &data);

	// clear screen (not cleared by reset)
	screen_clear();

	// screen goes to home on reset
	state.row = 0;
	state.col = 0;
}

void screen_clear()
{
	u8 data;

	// send clear instruction
	screen_wait();
	data = INS_CLEA;
	screen_io(WIR, &data);

	// clear buffer
	(void) memset(state.buffer, ' ', sizeof(state.buffer));
}

void screen_putc(u8 c, u16 flags)
{
	// column overflow
	if (state.col >= SCREEN_COLS) {
		if (flags & WRAPCOL) {
			state.col = 0;
			state.row++;
		} else {
			return;
		}
	}

	// row overflow
	if (state.row >= SCREEN_ROWS) {
		if (flags & WRAPROW) {
			state.row = 0;
		} else {
			return;
		}
	}

	// put character in buffer
	state.buffer[state.row][state.col++] = c;
}

void screen_puts(str s, u8 size, u16 flags)
{
	chr c;

	// puts() for null terminated string
	if (size == NULLTERM) {
		while (1)
		{
			// how to read character
			if (flags & ROMSTR)
				c = pgm_read_byte(s++);
			else
				c = *s++;

			// null terminator found
			if (c == 0)
				break;

			// put character on screen
			screen_putc(c, flags);
		}
	
	// puts() for string with size
	} else {
		for (u8 i = 0; i < size; i++)
		{
			// how to read character
			if (flags & ROMSTR)
				c = pgm_read_byte(&s[i]);
			else
				c = s[i];

			// put character on screen
			screen_putc(c, flags);
		}
	}
}

static const char nchars[16] PROGMEM = {
	'0', '1', '2', '3', 
	'4', '5', '6', '7', 
	'8', '9', 'A', 'B', 
	'C', 'D', 'E', 'F' 
};

#define digits(N, B) ((u8)(log((N))/log((B))) + 1)

void screen_puti(s32 i, u8 base, u16 flags)
{
	u8 sign = i < 0; // extract number sign

	i = (1 - 2*sign)*i; // convert to absolute value

	u16 len = 0;

	// calculate maximum length
	if (flags & I8)
		len = digits(((u8)(~0) >> 1), base);
	else if (flags & I16)
		len = digits(((u16)(~0) >> 1), base);
	else if (flags & I32)
		len = digits(((u32)(~0) >> 1), base);
	
	// if no padding, minimize length
	if (!(flags & (ZPAD | SPAD))) {
		u8 tmp = digits(i, base);
		if (tmp < len)
			len = tmp;
	}

	// sign requires one bit
	if (flags & SIGN)
		len++;

	// allocate buffer
	chr buf[len + 1];
	memset(buf, 0, len + 1);

	u8 a, b, c;

	// can't have zero base
	if (base == 0)
		return;

	// clamp base
	if (unlikely(base > length(nchars)))
		base = length(nchars);

	// conversion
	a = 0;
	do {
		if (unlikely(a >= len)) {
			if (likely(flags & TRUN)) {
				(void)memmove(buf, &buf[1], len - 1);
				a = len - 1;
			} else {
				break;
			}
		}

		buf[a++] = pgm_read_byte(&nchars[i % base]);
		i /= base;
	} while (i != 0);

	// zero pad
	if (flags & ZPAD) {
		b = len - !!(flags & SIGN);
		while (a < b)
			buf[a++] = '0';

	// space pad
	} else if (flags & SPAD) {
		b = len - !!(flags & SIGN);
		while (a < b)
			buf[a++] = ' ';
	}

	// sign
	if (flags & SIGN) {
		chr tmp;

		if (sign)
			tmp = '-';
		else if (flags & PLUS)
			tmp = '+';
		else
			tmp = ' ';

		buf[a++] = tmp;
	}

	// flip output
	b = 0;
	c = a / 2;
	while (b < c)
	{
		// swap
		chr c = buf[b];
		buf[b] = buf[a - b - 1];
		buf[a - b - 1] = c;
		b++;
	}

	// print to screen
	screen_puts(buf, a, flags);
}

void screen_cursor(u8 row, u8 col, cursor_t cursor)
{
	u8 data;
	s8 row_offset, col_offset;

	row_offset = (row >> 4) & 0x7;
	if (row & 0x80)
		row_offset = -row_offset;
	row &= OLDPOS;

	col_offset = (col >> 4) & 0x7;
	if (col & 0x80)
		col_offset = -col_offset;
	col &= OLDPOS;

	switch ((row == OLDPOS) | ((col == OLDPOS) << 1)) {
	// use the specified positions
	case 0:
known:
		data = INS_SDDA | DDRAM_ADDR(GET_ADDR(row, col));
		screen_wait();
		screen_io(WIR, &data);
		break;
	
	// need to get DDRAM address
	case 1:
	case 2:
address:
		screen_wait();
		screen_io(RIR, &data);
		data = DDRAM_ADDR(data);
		
		// deduce unknown position
		if (row == OLDPOS)
			row = (u8)((s8)ADDR_ROW(data) + row_offset);
		if (col == OLDPOS)
			col = (u8)((s8)ADDR_COL(data) + col_offset);

		goto known;

	// no need to change position
	case 3:
		if ((row_offset == 0) && (col_offset == 0))
			break;
		goto address;
	}

	// set cursor display mode
	data = INS_CTRL | FLG_CTRL_DISP;
	switch (cursor) {
	case NONE:
		break;
	case LINE:
		data |= FLG_CTRL_CURS;
		break;
	case RECT:
		data |= FLG_CTRL_BLNK;
		break;
	case BOTH:
		data |= FLG_CTRL_CURS | FLG_CTRL_BLNK;
		break;
	case OLD:
		return;
	}
	screen_wait();
	screen_io(WIR, &data);
}

void screen_goto(u8 row, u8 col)
{
	// actual instructions handled by screen_flush()
	state.row = row;
	state.col = col;
}

void screen_flush()
{
	u8 old_row, old_col, cur_row, cur_col;	

	// get DDRAM address
	screen_wait();
	screen_io(RIR, &old_row);
	old_row = DDRAM_ADDR(old_row);
	
	old_col = cur_col = ADDR_COL(old_row);
	old_row = cur_row = ADDR_ROW(old_row);

	for (u8 row = 0; row < SCREEN_ROWS; row++)
	{
		for (u8 col = 0; col < SCREEN_COLS; col++)
		{
			chr *src, *dst;
			src = &state.buffer[row][col];
			dst = &state.buffer[row + SCREEN_ROWS][col];

			// no change
			if (likely(*src == *dst))
				continue;
			
			// cursor must be moved (change DDRAM address)
			if (unlikely((cur_col != col) || (cur_row != row))) {
				u8 data = INS_SDDA | DDRAM_ADDR(GET_ADDR(row, col));
				screen_wait();
				screen_io(WIR, &data);
			}

			// write character on screen
			screen_wait();
			screen_io(WDR, (u8 *)src);
			*dst = *src;

			// screen increments column automatically
			cur_col++;
		}
	}

	// restore old cursor position
	old_row = INS_SDDA | DDRAM_ADDR(GET_ADDR(old_row, old_col));
	screen_wait();
	screen_io(WIR, &old_row);
}

// BACKLIGHT CONTROL

static u8 ticks;

void screen_backlight(bls_t state)
{
	switch (state) {
	case OFF:
		ev_set_id(SCREEN_BLINK, 1);
		PORTL &= ~_BV(3); // backlight off
		break;
	case ON:
		ev_set_id(SCREEN_BLINK, 1);
		PORTL |= _BV(3); // backlight on
		break;
	case BLINK:
		ev_set_id(SCREEN_BLINK, 0);
		ticks = SCREEN_BLINK_TICKS;
		break;
	}
}

u8 e_screen_blink(u8 unused id, u8 unused code, u8 unused arg)
{
	// wait for specified amount of ticks
	if (ticks-- > 0)
		return 0;
	ticks = SCREEN_BLINK_TICKS;

	PINL |= _BV(3); // toggle backlight

	return 0;
}

INIT()
{
	// data pins
	DDRC  = ~0;
	PORTC =  0;

	// control pins
	DDRL  |=  0xF;
	PORTL &= ~0xF;

	// set backlight on
	PORTL |= _BV(3);

	// intialize screen
	screen_reset();
}
