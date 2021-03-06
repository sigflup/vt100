/*
 * vt100.h	Header file for the vt100 emulator.
 *
 *		$Id: vt100.h,v 1.4 2007-10-10 20:18:20 al-guest Exp $
 *
 *		This file is part of the minicom communications package,
 *		Copyright 1991-1995 Miquel van Smoorenburg.
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* 05/09/2010 shoved head first into htpterm and flushed the handle- 
 * sigflup
 *
 * 08/17/2012 Made more portable, also now we operate on a character buffer
 * sigflup
 */

#ifndef __MINICOM__SRC__VT100_H__
#define __MINICOM__SRC__VT100_H__
#include <stdio.h>

#define ANSWERBACK_LEN 	89
#define CONVCAP_LEN	80

#define P_ANSWERBACK answerback
#define P_CONVCAP convcap

/* Keypad and cursor key modes. */
#define NORMAL	1
#define APPL	2

/* Don't change - hardcoded in minicom's dial.c */
#define VT100	1
#define ANSI	3
#define RAW	4

typedef struct {
 FILE *capfp;
 int vt_echo;
 int vt_nl_delay;		/* Delay after CR key */
 int vt_type;	/* Terminal type. */
 int vt_wrap;	/* Line wrap on/off */
 int vt_addlf;	/* Add linefeed on/off */
 int vt_fg;		/* Standard foreground color. */
 int vt_bg;		/* Standard background color. */
 int vt_keypad;		/* Keypad mode. */
 int vt_cursor;		/* cursor key mode. */
 int vt_asis;		/* 8bit clean mode. */
 int vt_bs;		/* Code that backspace key sends. */
 int vt_insert;	/* Insert mode */
 int vt_crlf;		/* Return sends CR/LF */
 int vt_om;		/* Origin mode. */
 struct term_t *vt_win;	/* Output window. */
 int vt_docap;		/* Capture on/off. */
 void (*termout)(const char *, int, struct term_t *);/* Gets called to output a string. */
 int enable_iconv;
 int escparms[8];		/* Cumulated escape sequence. */
 int ptr;		/* Index into escparms array. */
 long vt_tabs[5];		/* Tab stops for max. 32*5 = 160 columns. */
 short newy1;
 short newy2;
 short savex, savey, saveattr, savecol;
} state_t;

/* Prototypes from vt100.c */
void vt_install(void (*fun1)(const char *, int, struct term_t *), 
	        struct term_t *pwin);
void vt_init(struct term_t *, int, int, int, int, int);
void vt_pinit(struct term_t *, int, int);
void vt_set(struct term_t *, int, int, int, int, int, int, int);
void vt_out(struct term_t *, unsigned int);
void vt_send(struct term_t *, unsigned int ch);

#endif /* ! __MINICOM__SRC__VT100_H__ */

/*
 * Possible attributes.
 */
#define XA_NORMAL	 0
#define XA_BLINK	 1
#define XA_BOLD		 2
#define XA_REVERSE	 4
#define XA_STANDOUT	 8
#define XA_UNDERLINE	16
#define XA_ALTCHARSET	32
#define XA_BLANK	64

/*
 * Possible colors
 */
#define BLACK		0
#define RED		1
#define GREEN		2
#define YELLOW		3
#define BLUE		4
#define MAGENTA		5
#define CYAN		6
#define WHITE		7

#define COLATTR(fg, bg) (((fg) << 4) + (bg))
#define COLFG(ca)	((ca) >> 4)
#define COLBG(ca)	((ca) % 16)

/*
 * Possible borders.
 */
#define BNONE	0
#define BSINGLE 1
#define BDOUBLE 2

/*
 * Scrolling directions.
 */
#define S_UP	1
#define S_DOWN	2

/*
 * Cursor types.
 */
#define CNONE	0
#define CNORMAL	1

/*
 * Title Positions
 */
#define TLEFT	0
#define TMID	1
#define TRIGHT	2


/*
 * Allright, now the macro's for our keyboard routines.
 */

#define K_BS		8
#define K_ESC		27
#define K_STOP		256
#define K_F1		257
#define K_F2		258
#define K_F3		259
#define K_F4		260
#define K_F5		261
#define K_F6		262
#define K_F7		263
#define K_F8		264
#define K_F9		265
#define K_F10		266
#define K_F11		277
#define K_F12		278

#define K_HOME		267
#define K_PGUP		268
#define K_UP		269
#define K_LT		270
#define K_RT		271
#define K_DN		272
#define K_END		273
#define K_PGDN		274
#define K_INS		275
#define K_DEL		276

#define NUM_KEYS	23
#define KEY_OFFS	256

/* Here's where the meta keycode start. (512 + keycode). */
#define K_META		512

#ifndef EOF
#  define EOF		((int) -1)
#endif
#define K_ERA		'\b'
#define K_KILL		((int) -2)


#define term_wsetfgcol(w,fg) ( (w)->color = ((w)->color &15)+((fg)<<4))
#define term_wsetbgcol(w,bg) ( (w)->color = ((w)->color &240)+ (bg) )
#define term_wsetattr(w,a) ( (w)->attr = (a) )
#define term_wgetattr(w) ( (w)->attr )

#define term_wsetregion(w, z1, z2) (((w)->sy1=z1),((w)->sy2=z2)) 
#define term_wresetregion(w) ( (w)->sy1 = 0, (w)->sy2 = ((w)->ws_conf.ws_row-1))  


#define LEFT	0
#define RIGHT	1
#define UP	2
#define DOWN	3

#define BELL		7
#define BACKSPACE	8
#define TAB		'\t'
#define QUIT		3

#define MAX_AGE		4	


enum {
 DEC_1,
 DEC_6,
 DEC_7,
 DEC_25,
 DEC_47,
 DEC_67,
 DEC_SIZE
};

enum {
 CONTROL_BACKSPACE,
 CONTROL_TAB,
 CONTROL_TERM,
 CONTROL_STOP,
 CONTROL_CONTINUE,
 CONTROL_QUIT,
 CONTROL_DOWN,
 CONTROL_AAA,
 CONTROL_SIZE
};

typedef struct {
 unsigned char text, col, attrib;
} char_t;

typedef struct {
 void (*char_out)(char c, struct term_t *win);
 int cursor_on;
 int dec_mode[DEC_SIZE];
 unsigned char control_keys[CONTROL_SIZE];
 int color, attr;
 int cursor_x, cursor_y;
 int wrap, doscroll;
 int sy1, sy2;
 state_t state;
 struct winsize ws_conf;
 char_t *chars;
 int dirty;
} term_t;

void term_wscroll(term_t *win, int dir);
void term_wlocate(term_t *win, int x, int y);
void term_wputs(term_t *win, unsigned char *s);
void term_wclrch(term_t *win, int n);
void term_wclreol(term_t *win);
void term_wclrbol(term_t *win);
void term_wclrel(term_t *win);
void term_wclreos(term_t *win);
void term_wclrbos(term_t *win);
void term_winclr(term_t *win);
void term_winsline(term_t *win);
void term_wdelline(term_t *win);
void term_wdelchar(term_t *win);
void term_winschar(term_t *win, unsigned char c, int move);
void term_wcursor(term_t *win, int type);
void term_wputc(term_t *win, unsigned char c);
void term_wmove(term_t *win, int dir); 
void term_wredraw(term_t *win, int newdirect);

void init_term(struct term_t *win, void (*char_out)(char c, struct term_t *win),int  w, int h);

void resize_term(struct term_t *win, int w, int h);
