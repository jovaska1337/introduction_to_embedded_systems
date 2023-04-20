#ifndef SCREEN_H
#define SCREEN_H

#include "util/attr.h"
#include "util/type.h"

// how many ticks to wait before changing blink state
#define SCREEN_BLINK_TICKS 5

// LOW LEVEL IO

// see HD44780 datasheet for more info on these

#define INS_CLEA (1 << 0) // clear display
#define INS_HOME (1 << 1) // return home
#define INS_EMST (1 << 2) // entry mode set
#define INS_CTRL (1 << 3) // display on/off control
#define INS_SHFT (1 << 4) // cursor or display shift
#define INS_FSET (1 << 5) // function set
#define INS_SCGA (1 << 6) // set CGRAM address
#define INS_SDDA (1 << 7) // set DDRAM address

#define FLG_EMST_IORD (1 << 1) // cursor move direction right/left
#define FLG_EMST_SHFT (1 << 0) // accompanies display shift
#define FLG_CTRL_DISP (1 << 2) // display on/off
#define FLG_CTRL_CURS (1 << 1) // cursor underline on/off
#define FLG_CTRL_BLNK (1 << 0) // cursor blink on/off
#define FLG_SHFT_MOVE (1 << 3) // cursor shift/display shift
#define FLG_SHFT_RORL (1 << 2) // shift left/right
#define FLG_FSET_MODE (1 << 4) // 4-bit/8-bit mode
#define FLG_FSET_LINE (1 << 3) // 1/2 lines
#define FLG_FSET_FONT (1 << 2) // 5x8/5x10 font

#define CGRAM_ADDR(N) ((N) & 0x3F) // mask for CGRAM address
#define DDRAM_ADDR(N) ((N) & 0x7F) // mask for DDRAM address

//#define ADDR_ROW(N) ((N) / 64) // get row from DDRAM address
//#define ADDR_COL(N) ((N) % 16) // get column from DDRAM address
//#define GET_ADDR(R, C) (64*(R) + (C)) // convert row and column to DDRAM address

// these are faster, modulo is especially slow on AVR
#define ADDR_ROW(N) ((N) >> 6) // get row from DDRAM address
#define ADDR_COL(N) ((N) & 0xF) // get column from DDRAM address
#define GET_ADDR(R, C) (((R) << 6) + (C)) // convert row and column to DDRAM address

#define RIR_BF 0x80 // busy flag

// screen dimensions
#define SCREEN_COLS 16
#define SCREEN_ROWS 2
#define SCREEN_CHRS (SCREEN_COLS*SCREEN_ROWS)

// screen IO type
typedef enum { WIR = 0, WDR, RIR, RDR } packed sio_t;

// screen IO wrapper
void screen_io(sio_t type, u8 *data);

// wait for controller to clear busy flag
void screen_wait();

// BUFFERED HIGH LEVEL IO 

void screen_reset();

void screen_clear();

// values for flags
#define WRAPCOL (1 <<  0)
#define WRAPROW (1 <<  1)
#define ROMSTR  (1 <<  2)
#define I8      (1 <<  3)
#define I16     (1 <<  4)
#define I32     (1 <<  5)
#define SIGN    (1 <<  6)
#define PLUS    (1 <<  7)
#define ZPAD    (1 <<  8)
#define SPAD    (1 <<  9)
#define TRUN    (1 << 10)

void screen_putc(u8 c, u16 flags);

#define NULLTERM (u8)(~0) // pass for size when null terminated

void screen_puts(str s, u8 size, u16 flags);

void screen_puti(s32 i, u8 base, u16 flags);

typedef enum { NONE, LINE, RECT, BOTH, OLD } packed cursor_t;

#define OLDPOS 0xF 
#define OFFSET(N) ((((N) & 0x80) | ((((N) & 0x80 ? -(N) : (N)) & 0x7) << 4)) | OLDPOS)

void screen_cursor(u8 row, u8 col, cursor_t cursor);

void screen_goto(u8 row, u8 col);

void screen_flush();

// BACKLIGHT CONTROL

// scren backlight states
typedef enum { OFF = 0, ON, BLINK } packed bls_t;

// set screen backlight state
void screen_backlight(bls_t state);

#endif //!SCREEN_H
