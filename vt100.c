/*
 * vt100.c	ANSI/VT102 emulator code.
 *		This code was integrated to the Minicom communications
 *		package, but has been reworked to allow usage as a separate
 *		module.
 *
 *		It's not a "real" vt102 emulator - it's more than that:
 *		somewhere between vt220 and vt420 in commands.
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
 *
 * // jl 04.09.97 character map conversions in and out
 *    jl 06.07.98 use conversion tables with the capture file
 */

/* 05/09/2010 shoved head first into htpterm and flushed the handle- 
 * sigflup
 *
 * 08/17/2012 Made more portable, also now we operate on a character buffer
 * sigflup
 */

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>

#include "vt100.h"
#include "term.h"


#define	W ws_conf.ws_col
#define H ws_conf.ws_row

/* BUGS!!!!
 * Why is this above 0xff? See XXX:mappers 
 * back-tab \33[Z see XXX:bt
 * character sets see XXX:char sets 
 * */


/*
 * The global variable esc_s holds the escape sequence status:
 * 0 - normal
 * 1 - ESC
 * 2 - ESC [
 * 3 - ESC [ ?
 * 4 - ESC (
 * 5 - ESC )
 * 6 - ESC #
 * 7 - ESC P
 */
static int esc_s = 0;

#define ESC 27

/* Structure to hold escape sequences. */
struct escseq {
  int code;
  const char *vt100_st;
  const char *vt100_app;
  const char *ansi;
};

/* Escape sequences for different terminal types. */
 static struct escseq vt_keys[] = {
   { K_F1,	"OP",	"OP",	"OP" },
   { K_F2,	"OQ",	"OQ",	"OQ" },
   { K_F3,	"OR",	"OR",	"OR" },
   { K_F4,	"OS",	"OS",	"OS" },
   { K_F5,	"[16~",	"[16~",	"OT" },
   { K_F6,	"[17~",	"[17~",	"OU" },
   { K_F7,	"[18~",	"[18~",	"OV" },
   { K_F8,	"[19~",	"[19~",	"OW" },
   { K_F9,	"[20~",	"[20~",	"OX" },
   { K_F10,	"[21~",	"[21~",	"OY" },
   { K_F11,	"[23~",	"[23~",	"OY" },
   { K_F12,	"[24~",	"[24~",	"OY" },
   { K_HOME,	"[1~",	"[1~",	"[H" },
   { K_PGUP,	"[5~",	"[5~",	"[V" },
   { K_UP,	"[A",	"OA",	"[A" },
   { K_LT,	"[D",	"OD",	"[D" },
   { K_RT,	"[C",	"OC",	"[C" },
   { K_DN,	"[B",	"OB",	"[B" },
   { K_END,	"[4~",	"[4~",	"[Y" },
   { K_PGDN,	"[6~",	"[6~",	"[U" },
   { K_INS,	"[2~",	"[2~",	"[@" },
   { K_DEL,	"[3~",	"[3~",	"\177" },
   { 0,		NULL,	NULL,	NULL }
 };


 char answerback[ANSWERBACK_LEN] = { 0 };
 char convcap[CONVCAP_LEN] = { 0 };


 size_t one_mbtowc(char *pwc, char *s, size_t n) {
  int len;
  len = mbtowc((wchar_t *)pwc, s, n);
  if(len == -1)
   *pwc = *s;
  if(len<=0)
   len = 1;
  return len;
 }

 /*
  * Initialize the emulator once.
  */
 void vt_install(void (*fun1)(const char *, int, struct term_t *), 
		 struct term_t *pwin)
 {
  term_t *win = (term_t *)pwin;
  win->state.termout = fun1;
  win = win;
 }

 /* Partial init (after screen resize) */
 void vt_pinit(struct term_t *pwin, int fg, int bg)
 {
  term_t *win = (term_t *)pwin;
   win = win;
   win->state.newy1 = 0;
   win->state.newy2 = win->H - 1;
   term_wresetregion(win);
   if (fg > 0)
     win->state.vt_fg = fg;
   if (bg > 0)
     win->state.vt_bg = bg;
   term_wsetfgcol(win, win->state.vt_fg);
   term_wsetbgcol(win, win->state.vt_bg);
 }

 /* Set characteristics of emulator. */
 void vt_init(struct term_t *pwin, int type, int fg, int bg, int wrap, int add)
 {
  term_t *win = (term_t *)pwin;

  win->wrap = 0;
  win->state.vt_wrap = -1;
  win->state.vt_addlf = 0;
  win->state.vt_asis = 0;
  win->state.vt_bs = 8;
  win->state.vt_insert = 0;
  win->state.vt_crlf = 0;
  win->state.vt_docap = 0;
  win->state.enable_iconv = 0;
  win->state.ptr = 0;
  win->state.newy1 = 0;
  win->state.newy2 = win->ws_conf.ws_col;


  win->state.savex = 0; 
  win->state.savey = 0;
  win->state.saveattr = XA_NORMAL;
  win->state.savecol = 112;

   win->state.vt_type = type;
   if (win->state.vt_type == ANSI) {
	 win->state.vt_fg = WHITE;
	 win->state.vt_bg = BLACK;
   } else {
	 win->state.vt_fg = fg;
	 win->state.vt_bg = bg;
   }
   if (wrap >= 0)
     win->wrap = win->state.vt_wrap = wrap;
   win->state.vt_addlf = add;
   win->state.vt_insert = 0;
   win->state.vt_crlf = 0;
   win->state.vt_om = 0;

   win->state.newy1 = 0;
   win->state.newy2 = win->H - 1;
   term_wresetregion(win);
   win->state.vt_keypad = NORMAL;
   win->state.vt_cursor = NORMAL;
   win->state.vt_echo = 0;
   win->state.vt_tabs[0] = 0x01010100;
   win->state.vt_tabs[1] =
   win->state.vt_tabs[2] =
   win->state.vt_tabs[3] =
   win->state.vt_tabs[4] = 0x01010101;
   win->state.ptr = 0;
   memset(win->state.escparms, 0, sizeof(win->state.escparms));
   esc_s = 0;

   term_wsetfgcol(win, win->state.vt_fg);
   term_wsetbgcol(win, win->state.vt_bg);
 }

 /* Change some things on the fly. */
 void vt_set(struct term_t *pwin, int addlf, int wrap, int docap, int bscode,
	     int echo, int cursor, int asis)
 {
  term_t *win = (term_t *)pwin;
   if (addlf >= 0)
     win->state.vt_addlf = addlf;
   if (wrap >= 0)
     win->wrap = win->state.vt_wrap = wrap;
   if (docap >= 0)
     win->state.vt_docap = docap;
   if (bscode >= 0)
     win->state.vt_bs = bscode;
   if (echo >= 0)
     win->state.vt_echo = echo;
   if (cursor >= 0)
     win->state.vt_cursor = cursor;
   if (asis >=0)
     win->state.vt_asis = asis;
 }

 /* Output a string to the modem. */
 static void v_termout(term_t *win, const char *s, int len)
 {
   const char *p;

   if (win->state.vt_echo) {
     for (p = s; *p; p++) {
       vt_out((struct term_t *)win, *p);
       if (!win->state.vt_addlf && *p == '\r')
	 vt_out((struct term_t *)win, '\n');
     }
   }
   (*win->state.termout)(s, len, (struct term_t *)win);
 }

 /*
  * Escape code handling.
  */

 /*
  * ESC was seen the last time. Process the next character.
  */
 static void state1(term_t *win, int c)
 {
   short x, y, f;

   switch(c) {
     case '[': /* ESC [ */
       esc_s = 2;
       return;
     case '(': /* ESC ( */
       esc_s = 4;
       return;
     case ')': /* ESC ) */
      esc_s = 5;
      return;
    case '#': /* ESC # */
      esc_s = 6;
      return;
    case 'P': /* ESC P (DCS, Device Control String) */
      esc_s = 7;
      return;
    case 'D': /* Cursor down */
    case 'M': /* Cursor up */
      x = win->cursor_x;
      if (c == 'D') { /* Down. */
        y = win->cursor_y + 1;
        if (y == win->state.newy2 + 1)
          term_wscroll(win, S_UP);
        else if (win->cursor_y < win->H)
          term_wlocate(win, x, y);
      }
      if (c == 'M')  { /* Up. */
        y = win->cursor_y - 1;
        if (y == win->state.newy1 - 1)
          term_wscroll(win, S_DOWN);
        else if (y >= 0)
          term_wlocate(win, x, y);
      }
      break;
    case 'E': /* CR + NL */
      term_wputs(win, (unsigned char *)"\r\n");
      break;
    case '7': /* Save attributes and cursor position */
    case 's':
      win->state.savex = win->cursor_x;
      win->state.savey = win->cursor_y;
      win->state.saveattr = win->attr;
      win->state.savecol = win->color;
      break;
    case '8': /* Restore them */
    case 'u':
      win->color = win->state.savecol; /* HACK should use mc_wsetfgcol etc */
      term_wsetattr(win, win->state.saveattr);
      term_wlocate(win, win->state.savex, win->state.savey);
      break;
    case '=': /* Keypad into applications mode */
      win->state.vt_keypad = APPL;
      break;
    case '>': /* Keypad into numeric mode */
      win->state.vt_keypad = NORMAL;
      break;
    case 'Z': /* Report terminal type */
      if (win->state.vt_type == VT100)
        v_termout(win, "\033[?1;0c", 0);
      else
        v_termout(win, "\033[?c", 0);
      break;
    case 'c': /* Reset to initial state */
      f = XA_NORMAL;
      term_wsetattr(win, f);
      win->wrap = (win->state.vt_type != VT100);
      if (win->state.vt_wrap != -1)
        win->wrap = win->state.vt_wrap;
      win->state.vt_crlf = win->state.vt_insert = 0;
      vt_init((struct term_t *)win, win->state.vt_type, win->state.vt_fg, 
	      win->state.vt_bg, win->wrap, 0);
      term_wlocate(win, 0, 0);
      break;
    case 'H': /* Set tab in current position */
      x = win->cursor_x;
      if (x > 159)
        x = 159;
      win->state.vt_tabs[x / 32] |= 1 << (x % 32);
      break;
    case 'N': /* G2 character set for next character only*/
    case 'O': /* G3 "				"    */
    case '<': /* Exit vt52 mode */
    default:
      /* ALL IGNORED */
      break;
  }
  esc_s = 0;
}

/* ESC [ ... [hl] seen. */
static void ansi_mode(term_t *win, int on_off)
{
  int i;

  for (i = 0; i <= win->state.ptr; i++) {
    switch (win->state.escparms[i]) {
      case 4: /* Insert mode  */
        win->state.vt_insert = on_off;
        break;
      case 20: /* Return key mode */
        win->state.vt_crlf = on_off;
        break;
    }
  }
}

/*
 * ESC [ ... was seen the last time. Process next character.
 */
static void state2(term_t *win, int c)
{
  short x, y, attr, f;
  char temp[32];

  /* See if a number follows */
  if (c >= '0' && c <= '9') {
    win->state.escparms[win->state.ptr] = 10*win->state.escparms[win->state.ptr] + c - '0';
    return;
  }
  /* Separation between numbers ? */
  if (c == ';') {
    if (win->state.ptr < 15)
      win->state.ptr++;
    return;
  }
  /* ESC [ ? sequence */
  if (win->state.escparms[0] == 0 && win->state.ptr == 0 && c == '?')
  {
    esc_s = 3;
    return;
  }

  /* Process functions with zero, one, two or more arguments */
  switch (c) {
    case 'A':
    case 'B':
    case 'C':
    case 'D': /* Cursor motion */
      if ((f = win->state.escparms[0]) == 0)
        f = 1;
      x = win->cursor_x;
      y = win->cursor_y;
      x += f * ((c == 'C') - (c == 'D'));
      if (x < 0)
        x = 0;
      if (x >= win->W)
        x = win->W - 1;
      if (c == 'B') { /* Down. */
        y += f;
        if (y >= win->H)
          y = win->H - 1;
        if (y >= win->state.newy2 + 1)
          y = win->state.newy2;
      }
      if (c == 'A') { /* Up. */
        y -= f;
        if (y < 0)
          y = 0;
        if (y <= win->state.newy1 - 1)
          y = win->state.newy1;
      }	
      term_wlocate(win, x, y);
      break;
    case 'X': /* Character erasing (ECH) */
      if ((f = win->state.escparms[0]) == 0)
        f = 1;
      term_wclrch(win, f);
      break;
    case 'K': /* Line erasing */
      switch (win->state.escparms[0]) {
        case 0:
          term_wclreol(win);
          break;
        case 1:
          term_wclrbol(win);
          break;
        case 2:
          term_wclrel(win);
          break;
      }
      break;
    case 'J': /* Screen erasing */
      x = win->color;
      y = win->attr;
      if (win->state.vt_type == ANSI) {
        term_wsetattr(win, XA_NORMAL);
        term_wsetfgcol(win, WHITE);
        term_wsetbgcol(win, BLACK);
      }
      switch (win->state.escparms[0]) {
        case 0:
          term_wclreos(win);
          break;
        case 1:
          term_wclrbos(win);
          break;
        case 2:
          term_winclr(win);
          break;
      }
      if (win->state.vt_type == ANSI) {
        win->color = x;
        win->attr = y;
      }
      break;
    case 'n': /* Requests / Reports */
      switch(win->state.escparms[0]) {
        case 5: /* Status */
          v_termout(win, "\033[0n", 0);
          break;
        case 6:	/* Cursor Position */
          snprintf(temp, 32, "\033[%d;%dR", win->cursor_y + 1, win->cursor_x + 1);
          v_termout(win, temp, 0);
          break;
      }
      break;
    case 'c': /* Identify Terminal Type */
      if (win->state.vt_type == VT100) {
        v_termout(win, "\033[?1;2c", 0);
        break;
      }
      v_termout(win, "\033[?c", 0);
      break;
    case 'x': /* Request terminal parameters. */
      /* Always answers 19200-8N1 no options. */
      snprintf(temp, 32, "\033[%c;1;1;120;120;1;0x", win->state.escparms[0] == 1 ? '3' : '2');
      v_termout(win, temp, 0);
      break;
    case 's': /* Save attributes and cursor position */
      win->state.savex = win->cursor_x;
      win->state.savey = win->cursor_y;
      win->state.saveattr = win->attr;
      win->state.savecol = win->color;
      break;
    case 'u': /* Restore them */
      term_wsetfgcol(win, win->state.savecol);
      term_wsetattr(win, win->state.saveattr);
      term_wlocate(win, win->state.savex, win->state.savey);
      break;
    case 'h':
      ansi_mode(win, 1);
      break;
    case 'l':
      ansi_mode(win, 0);
      break;
    case 'H':
    case 'f': /* Set cursor position */
      if ((y = win->state.escparms[0]) == 0)
        y = 1;
      if ((x = win->state.escparms[1]) == 0)
        x = 1;
      if (win->state.vt_om)
        y += win->state.newy1;
      term_wlocate(win, x - 1, y - 1);
      break;
    case 'g': /* Clear tab stop(s) */
      if (win->state.escparms[0] == 0) {
        x = win->cursor_x;
        if (x > 159)
          x = 159;
        win->state.vt_tabs[x / 32] &= ~(1 << x % 32);
      }
      if (win->state.escparms[0] == 3)
        for(x = 0; x < 5; x++)
          win->state.vt_tabs[x] = 0;
      break;
    case 'm': /* Set attributes */
      attr = term_wgetattr((win));
      for (f = 0; f <= win->state.ptr; f++) {
        if (win->state.escparms[f] >= 30 && win->state.escparms[f] <= 37)
          term_wsetfgcol(win, win->state.escparms[f] - 30);
        if (win->state.escparms[f] >= 40 && win->state.escparms[f] <= 47)
          term_wsetbgcol(win, win->state.escparms[f] - 40);
        switch (win->state.escparms[f]) {
          case 0:
            attr = XA_NORMAL;
            term_wsetfgcol(win, win->state.vt_fg);
            term_wsetbgcol(win, win->state.vt_bg);
            break;
          case 4:
            attr |= XA_UNDERLINE;
            break;
          case 7:
            attr |= XA_REVERSE;
            break;
          case 1:
            attr |= XA_BOLD;
            break;
          case 5:
            attr |= XA_BLINK;
            break;
          case 22: /* Bold off */
            attr &= ~XA_BOLD;
            break;
          case 24: /* Not underlined */
            attr &=~XA_UNDERLINE;
            break;
          case 25: /* Not blinking */
            attr &= ~XA_BLINK;
            break;
          case 27: /* Not reverse */
            attr &= ~XA_REVERSE;
            break;
          case 39: /* Default fg color */
            term_wsetfgcol(win, win->state.vt_fg);
            break;
          case 49: /* Default bg color */
            term_wsetbgcol(win, win->state.vt_bg);
            break;
        }
      }
      term_wsetattr(win, attr);
      break;
    case 'L': /* Insert lines */
      if ((x = win->state.escparms[0]) == 0)
        x = 1;
      for (f = 0; f < x; f++)
        term_winsline(win);
      break;
    case 'M': /* Delete lines */
      if ((x = win->state.escparms[0]) == 0)
        x = 1;
      for (f = 0; f < x; f++)
        term_wdelline(win);
      break;
    case 'P': /* Delete Characters */
      if ((x = win->state.escparms[0]) == 0)
        x = 1;
      for (f = 0; f < x; f++)
        term_wdelchar(win);
      break;
    case '@': /* Insert Characters */
      if ((x = win->state.escparms[0]) == 0)
        x = 1;
      for (f = 0; f < x; f++)
        term_winschar(win, ' ', 0);
      break;
    case 'r': /* Set scroll region */
      if ((win->state.newy1 = win->state.escparms[0]) == 0)
        win->state.newy1 = 1;
      if ((win->state.newy2 = win->state.escparms[1]) == 0)
        win->state.newy2 = win->H;
      win->state.newy1-- ; win->state.newy2--;
      if (win->state.newy1 < 0)
        win->state.newy1 = 0;
      if (win->state.newy2 < 0)
        win->state.newy2 = 0;
      if (win->state.newy1 >= win->H)
        win->state.newy1 = win->H - 1;
      if (win->state.newy2 >= win->H)
        win->state.newy2 = win->H - 1;
      if (win->state.newy1 >= win->state.newy2) {
        win->state.newy1 = 0;
        win->state.newy2 = win->H - 1;
      }
      if(win->state.newy1 > win->state.newy2)
       term_wsetregion(win, win->state.newy2, win->state.newy1);
      else
       term_wsetregion(win, win->state.newy1, win->state.newy2);
      /// XXX xterm goes to 0,0 ?
      term_wlocate(win, 0, win->state.newy1);
      break;
    case 'i': /* Printing */
    case 'y': /* Self test modes */
    case 'Z':
      /* XXX:bt */
      break;
    default:
      /* IGNORED */
      break;
  }
  /* Ok, our escape sequence is all done */
  esc_s = 0;
  win->state.ptr = 0;
  memset(win->state.escparms, 0, sizeof(win->state.escparms));
  return;
}

/* ESC [? ... [hl] seen. */
static void dec_mode(term_t *win, int on_off)
{
  int i;
  for (i = 0; i <= win->state.ptr; i++) {
    switch (win->state.escparms[i]) {
      case 1: /* Cursor keys in cursor/appl mode */
        win->state.vt_cursor = on_off ? APPL : NORMAL;
	win->dec_mode[DEC_1] = on_off ? 1 : 0;
        break;
      case 6: /* Origin mode. */
        win->state.vt_om = on_off;
        term_wlocate(win, 0, win->state.newy1);
        break;
      case 7: /* Auto wrap */
        win->wrap = on_off;
        break;
      case 25: /* Cursor on/off */
        term_wcursor(win, on_off ? CNORMAL : CNONE);
        break;
      case 47:
	break;
      case 67: /* Backspace key sends. (FIXME: vt420) */
        /* setbackspace(on_off ? 8 : 127); */
        break;
      default: /* Mostly set up functions */
        /* IGNORED */
        break;
    }
  }
}

/*
 * ESC [ ? ... seen.
 */
static void state3(term_t *win, int c)
{
  /* See if a number follows */
  if (c >= '0' && c <= '9') {
    win->state.escparms[win->state.ptr] = 10*win->state.escparms[win->state.ptr] + c - '0';
    return;
  }
  switch (c) {
    case 'h':
      dec_mode(win, 1);
      break;
    case 'l':
      dec_mode(win, 0);
      break;
    case 'i': /* Printing */
    case 'n': /* Request printer status */
    default:
      /* IGNORED */
      break;
  }
  esc_s = 0;
  win->state.ptr = 0;
  memset(win->state.escparms, 0, sizeof(win->state.escparms));
  return;
}

/*
 * ESC ( Seen.
 */
static void state4(term_t *win, int c)
{
  /* Switch Character Sets. */
  switch (c) {
    case 'A':
    case 'B':
      break;
    case '0':
    case 'O':
      break;
  }
  esc_s = 0;
}

/*
 * ESC ) Seen.
 */
/* XXX:char set */
static void state5(term_t *win, int c)
{
  /* Switch Character Sets. */
  switch (c) {
    case 'A':
    case 'B':
      break;
    case 'O':
    case '0':
      /* default character set */
      break;
  }
  esc_s = 0;
}

/*
 * ESC # Seen.
 */
static void state6(term_t *win, int c)
{
  int x, y;

  /* Double height, double width and selftests. */
  switch (c) {
    case '8':
      /* Selftest: fill screen with E's */
      win->doscroll = 0;
      term_wlocate(win, 0, 0);
      for (y = 0; y < win->H; y++) {
        term_wlocate(win, 0, y);
        for (x = 0; x < win->W; x++)
          term_wputc(win, 'E');
      }
      term_wlocate(win, 0, 0);
      win->doscroll = 1;
      term_wredraw(win, 1);
      break;
    default:
      /* IGNORED */
      break;
  }
  esc_s = 0;
}

/*
 * ESC P Seen.
 */
static void state7(term_t *win, int c)
{
  /*
   * Device dependant control strings. The Minix virtual console package
   * uses these sequences. We can only turn cursor on or off, because
   * that's the only one supported in termcap. The rest is ignored.
   */
  static char buf[17];
  static int pos = 0;
  static int state = 0;

  if (c == ESC) {
    state = 1;
    return;
  }
  if (state == 1) {
    buf[pos] = 0;
    pos = 0;
    state = 0;
    esc_s = 0;
    if (c != '\\')
      return;
    /* Process string here! */
    if (!strcmp(buf, "cursor.on"))
      term_wcursor(win, CNORMAL);
    if (!strcmp(buf, "cursor.off"))
      term_wcursor(win, CNONE);
    if (!strcmp(buf, "linewrap.on")) {
      win->state.vt_wrap = -1;
      win->wrap = 1;
    }
    if (!strcmp(buf, "linewrap.off")) {
      win->state.vt_wrap = -1;
      win->wrap = 0;
    }
    return;
  }
  if (pos > 15)
    return;
  buf[pos++] = c;
}

void vt_out(struct term_t *pwin, unsigned int ch)
{
  int f;
  unsigned char c;
  int go_on = 0;
  wchar_t wc;
  term_t *win;

  win = (term_t *)pwin;
  if (!ch)
    return;


  c = (unsigned char)ch;

  if (win->state.vt_docap == 2) /* Literal. */
    fputc(c, win->state.capfp);

  /* Process <31 chars first, even in an escape sequence. */
  switch (c) {
    case '\r': /* Carriage return */
      term_wputc(win, c);
      if (win->state.vt_addlf) {
        term_wputc(win, '\n');
        if (win->state.vt_docap == 1)
          fputc('\n', win->state.capfp);
      }
      break;
    case '\t': /* Non - destructive TAB */
//      printf("tab pants\n");
      /* Find next tab stop. */
      for (f = win->cursor_x + 1; f < 160; f++)
        if (win->state.vt_tabs[f / 32] & (1 << f % 32))
          break;
      if (f >= win->W)
        f = win->W - 1;
      term_wlocate(win, f, win->cursor_y);
      if (win->state.vt_docap == 1)
        fputc(c, win->state.capfp);
      break;
    case 013: /* Old Minix: CTRL-K = up */
      term_wlocate(win, win->cursor_x, win->cursor_y - 1);
      break;
    case '\f': /* Form feed: clear screen. */
      term_winclr(win);
      term_wlocate(win, 0, 0);
      break;
    case 14:
      break;
    case 15:
      break;
    case 24:
    case 26:  /* Cancel escape sequence. */
      esc_s = 0;
      break;
    case ESC: /* Begin escape sequence */
      esc_s = 1;
      break;
    case 128+ESC: /* Begin ESC [ sequence. */
      esc_s = 2;
      break;
    case '\n':
    case '\b':
    case 7: /* Bell */
      term_wputc(win, c);
      if (win->state.vt_docap == 1)
        fputc(c, win->state.capfp);
      break;
    default:
      go_on = 1;
      break;
  }
  if (!go_on)
    return;
 

  /* Now see which state we are in. */
  switch (esc_s) {
    case 0: /* Normal character */
      /* XXX:mappers */
      c&=0xff;

      if (!win->state.enable_iconv) {
        one_mbtowc ((char *)&wc, (char *)&c, 1); 
        if (win->state.vt_insert)
          term_winschar(win, wc, 1);
        else
          term_wputc(win, wc);
      } else 
        term_wputc(win, c);
     
      break;
    case 1: /* ESC seen */
      state1(win, c);
      break;
    case 2: /* ESC [ ... seen */
      state2(win, c);
      break;
    case 3:
      state3(win, c);
      break;
    case 4:
      state4(win, c);
      break;
    case 5:
      state5(win, c);
      break;
    case 6:
      state6(win, c);
      break;
    case 7:
      state7(win,c);
      break;
  }

}

/* Translate keycode to escape sequence. */
void vt_send(struct term_t *pwin, unsigned int c)
{
 term_t *win = (term_t *)pwin;
  char s[3];
  int f;
  int len = 1;

  /* Special key? */
  if (c < 256) {
    /* Translate backspace key? */
    if (c == K_ERA)
      c = win->state.vt_bs;
    s[0] = c;
    s[1] = 0;
    /* CR/LF mode? */
    if (c == '\r' && win->state.vt_crlf) {
      s[1] = '\n';
      s[2] = 0;
      len = 2;
    }
    v_termout(win, s, len);
    if (win->state.vt_nl_delay > 0 && c == '\r')
      usleep(1000 * win->state.vt_nl_delay);
    return;
  }

  /* Look up code in translation table. */
  for (f = 0; vt_keys[f].code; f++)
    if (vt_keys[f].code == c)
      break;
  if (vt_keys[f].code == 0)
    return;

  /* Now send appropriate escape code. */
  v_termout(win, "\33", 0);
  if (win->state.vt_type == VT100) {
    if (win->state.vt_cursor == NORMAL)
      v_termout(win, vt_keys[f].vt100_st, 0);
    else
      v_termout(win, vt_keys[f].vt100_app, 0);
  } else
    v_termout(win, vt_keys[f].ansi, 0);
}


#undef W
#undef H

#define W win->ws_conf.ws_col
#define H win->ws_conf.ws_row

/* insert a character at cursor position */
/* XXX:winschar */
void term_winschar(term_t *win, unsigned char c, int move) {
 int i;

#ifdef DEBUG
 printf("term_winschar\n");
#endif

 for(i=win->ws_conf.ws_col-1;i!=win->cursor_x;i--) {
  win->chars[i+(win->cursor_y*win->ws_conf.ws_col)+1].text =
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) ].text; 
  win->chars[ i + (win->cursor_y * win->ws_conf.ws_col) +1 ].col =
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) ].col; 
  win->chars[ i + (win->cursor_y * win->ws_conf.ws_col) +1 ].attrib =
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) ].attrib; 
 }
 win->chars[win->cursor_x+(win->cursor_y*win->ws_conf.ws_col)].text = c;
 win->chars[   win->cursor_x + (win->cursor_y * win->ws_conf.ws_col)].col = 
  win->color;
 win->chars[win->cursor_x + (win->cursor_y * win->ws_conf.ws_col)].attrib = 
  win->attr;

 term_wmove(win, RIGHT);
}

/* print a character */
void term_wputc(term_t *win, unsigned char c) {

#ifdef DEBUG
 printf("term_wputc  %x\n", c);
#endif

 switch(c) {
  /* XXX:vt_out you */
  case '\r':
   win->cursor_x = 0;
   break;
  case '\n':
//   win->cursor_x = 0;
   term_wmove(win, DOWN);
   break;
  case BELL:
   /* XXX add bell things */
   break;
  default:
   if(c == win->control_keys[CONTROL_BACKSPACE]) {
    term_wmove(win, LEFT);
    break;
   }
   if(c<32) break;
   win->chars[win->cursor_x + (win->cursor_y * win->ws_conf.ws_col)].text = c;
   win->chars[win->cursor_x + (win->cursor_y * win->ws_conf.ws_col)].col =
    win->color;
   win->chars[win->cursor_x+(win->cursor_y*win->ws_conf.ws_col)].attrib=
    win->attr;
   term_wmove(win, RIGHT);
   break;
 }
 win->dirty = 1;
}

void term_wmove(term_t *win, int dir) {

#ifdef DEBUG
 printf("term_wmove\n");
#endif

 switch(dir) {
  case LEFT:
   win->cursor_x--;
   if(win->cursor_x < 0) 
    win->cursor_x = 0;
   break;
  case RIGHT:
   win->cursor_x++;
   if(win->cursor_x>=win->ws_conf.ws_col) { 
    if(win->wrap ==1 ) {
     win->cursor_x = 0;
     term_wmove(win, DOWN);
    } else
     win->cursor_x = win->ws_conf.ws_col-1; 
   }
   break;
  case UP:
   win->cursor_y--;
   if(win->cursor_y<win->sy1)
    win->cursor_y = win->sy1;
   break;
  case DOWN:
   win->cursor_y++;
   if(win->cursor_y>win->sy2) {
    term_wscroll(win, S_DOWN);
    win->cursor_y = win->sy2;
   }
   break;
 }
 win->dirty = 1;
}

/* locate the cursor */
void term_wlocate(term_t *win, int x, int y) {

#ifdef DEBUG
 printf("term_wlocate\n");
#endif

 if( (x >= 0 || x <= win->ws_conf.ws_col) &&
     (y >= 0 || y <= win->ws_conf.ws_row)) {
  win->cursor_x = x;
  win->cursor_y = y;
 }
 win->dirty = 1;
}

/* redraw the term */
void term_wredraw(term_t *win, int newdirect) {
 printf("wredraw\n");
 win->dirty = 1; 
}

/* flush output */
void term_wflush(term_t *win) {
#ifdef DEBUG
 printf("term_wflush\n");
#endif

 
}

/* clear the window */
void term_winclr(term_t *win) {
 int q;

#ifdef DEBUG
 printf("term_winclr\n");
#endif

 for(q = 0;q<(win->ws_conf.ws_row * win->ws_conf.ws_col);q++) {
  win->chars[q].text=
   win->chars[q].col=
   win->chars[q].attrib=0;
 }
}

/* scroll window */
void term_wscroll(term_t *win, int dir) {
 int x,y;
 int store;


 if(win->sy1 > win->sy2) {
  store = win->sy1;
  win->sy1 = win->sy2;
  win->sy2 = store;
 }
 if(dir == S_DOWN) { 
  for(y=win->sy1;y<win->sy2;y++) 
   for(x = 0;x<win->ws_conf.ws_col;x++) {
    win->chars[x+(y*W)].text=win->chars[x+((y+1)*W)].text;
    win->chars[  x + (y * W)].col =  win->chars[  x + ((y+1) * W)].col;
    win->chars[ x + (y * W)].attrib = win->chars[ x + ((y+1) * W)].attrib;
   }
  for( x = 0;x<win->ws_conf.ws_col;x++)  {
   win->chars[ x + (W * win->sy2)].text =
    win->chars[  x + (W * win->sy2)].col=
    win->chars[ x + (W * win->sy2)].attrib = 0;
  }


 } else {
  /* scroll UP */
  for(y=win->sy2;y>win->sy1;y--) {
   for(x = 0;x < win->ws_conf.ws_col;x++) {
    win->chars[x+(y*W)].text = win->chars[x +((y-1) * W)].text;
    win->chars[  x + (y * W)].col = win->chars[ x +((y-1) * W)].col;
    win->chars[x+(y * W)].attrib =  win->chars[x +((y-1) * W)].attrib;
   } 
  }
  for(x = 0; x< win->ws_conf.ws_col;x++) {
   win->chars[x + (W * win->sy1)].text=
    win->chars[ x + (W * win->sy1)].col=
    win->chars[x+(W * win->sy1)].attrib = 0;
  }
 }
 win->dirty = 1;
}

/* clear to end of line */
void term_wclreol(term_t *win) {
 int x;

#ifdef DEBUG
 printf("term_wclreol\n");
#endif

 for(x = win->cursor_x; x<win->ws_conf.ws_col;x++) {
  win->chars[  x + (win->cursor_y * win->ws_conf.ws_col)].text=
   win->chars[   x + (win->cursor_y * win->ws_conf.ws_col)].col=
   win->chars[x + (win->cursor_y * win->ws_conf.ws_col)].attrib=0;
 }
}

/* clear to beginning of line */
/* XXX:clrbol */
void term_wclrbol(term_t *win) {
 int x;

#ifdef DEBUG
 printf("term_wclrbol\n");
#endif

 for(x=win->ws_conf.ws_col;x>0;x--) {
  win->chars[x].text=
   win->chars[x].col=
   win->chars[x].attrib = 0;
 }
}

/* clear to end of screen */
void term_wclreos(term_t *win) {
 int x;

#ifdef DEBUG
 printf("term_wclreos\n");
#endif

 for(x= win->cursor_x + ( win->cursor_y * win->ws_conf.ws_col);
     x< (win->ws_conf.ws_row * win->ws_conf.ws_col);x++) {
  win->chars[x].text=
   win->chars[x].col=
   win->chars[x].attrib = 0;
 }
}

/* clear to beginning of screen */
void term_wclrbos(term_t *win) {
 int x;

#ifdef DEBUG
 printf("term_wclrbos\n");
#endif

 for(x = win->cursor_x + (win->cursor_y * win->ws_conf.ws_col);x>0;x--) {
  win->chars[x].text=
   win->chars[x].col=
   win->chars[x].attrib = 0;
 }
}

/* clear entire line */
void term_wclrel(term_t *win) {
 int x;

#ifdef DEBUG
 printf("term_wclrel\n");
#endif

 for(x = 0; x < win->cursor_x; x++) {
  win->chars[x].text=
   win->chars[x].col=
   win->chars[x].attrib = 0;
 }
}

/* insert? */
void term_winsline(term_t *win) {
 int store;

#ifdef DEBUG
 printf("term_winsline\n");
#endif

 if(win->cursor_y >= win->sy2) return;
 store = win->sy1;
 win->sy1 = win->cursor_y;
 term_wscroll(win, S_UP);
 win->sy1 = store;
}

/* delete line */
void term_wdelline(term_t *win) {
 int store;
#ifdef DEBUG
 printf("term_wdelline\n");
#endif


 if(win->cursor_y >= win->sy2) return;
 store = win->sy1;
 win->sy1 = win->cursor_y;
 term_wscroll(win, S_DOWN);
 win->sy1 = store;
}

/* delete char under cursor */
void term_wdelchar(term_t *win) {
 int i;
#ifdef DEBUG
 printf("term_wdelchar\n");
#endif


 for(i=win->cursor_x;i<win->ws_conf.ws_col-1;i++)
  win->chars[ i + (win->cursor_y * win->ws_conf.ws_col)].text =  
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) + 1].text;
  win->chars[ i + (win->cursor_y * win->ws_conf.ws_col)].col = 
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) + 1].col;
  win->chars[ i + (win->cursor_y * win->ws_conf.ws_col)].attrib = 
   win->chars[i + (win->cursor_y * win->ws_conf.ws_col) + 1].attrib;

 win->chars[win->ws_conf.ws_col+(win->cursor_y*win->ws_conf.ws_col)-1].text=0;
 win->chars[win->ws_conf.ws_col+(win->cursor_y*win->ws_conf.ws_col)-1].col=0;
 win->chars[win->ws_conf.ws_col+(win->cursor_y*win->ws_conf.ws_row)-1].attrib=0;
}

/* clear characters */
void term_wclrch(term_t *win, int n) {
 printf("wclrch\n");
}

/* set cursor type */
void term_wcursor(term_t *win, int type) {
#ifdef DEBUG
 printf("term_wcursor\n");
#endif
 
 /* XXX not sure what this does we have 
  * CNONE and CNORMAL */
 if(type == CNONE) 
  win->dec_mode[DEC_25] = 0;
 else
  win->dec_mode[DEC_25] = 1;
}

/* edit one line in term */
void term_wgetwcs(term_t *win, unsigned char *s, int linelen, int maxle) {
 printf("wgetwcs\n");
}

/* print a string */
void term_wputs(term_t *win, unsigned char *s) {
#ifdef DEBUG
 printf("term_wputs %s\n", s);
#endif
 unsigned char d;
 for(;;) {
  if( (d = *s++) == 0) break;
  win->char_out((char )d,(struct term_t *)win); 
 }
}

void keycodes_out(const char *dat, int len, struct term_t *win) {
 int i;
 term_t *pwin = (term_t *)win;
 if(len==0)
  term_wputs((term_t *)win, (unsigned char *)dat);
 else {
  for(i=0;i<len;i++)
   pwin->char_out((char)dat[i], win);
 }
}

void init_term(struct term_t *win, void (*char_out)(char c, struct term_t *win),int  w, int h) {
 term_t *pwin = (term_t *)win;
 pwin->ws_conf.ws_col = w;
 pwin->ws_conf.ws_row = h;
 pwin->chars = (char_t *)malloc(w*h*sizeof(char_t));
 bzero(pwin->chars, w*h*sizeof(char_t));
 pwin->char_out = char_out;
 pwin->cursor_x = 0;
 pwin->cursor_y = 0;
 pwin->dirty = 1;
 pwin->sy1 = 0;
 pwin->sy2 = h-1;

 pwin->color = 0;
 pwin->attr = 0;

 vt_init(win, ANSI, 0,0,1,0);
 vt_install(keycodes_out, win);
}


void resize_term(struct term_t *win, int w, int h) {
 term_t *pwin = (term_t *)win;
 int x, y;
 char_t *old_chars;
 unsigned char text, col, attrib;

 old_chars = pwin->chars;
 pwin->chars = (char_t *)malloc( w*h*sizeof(char_t));
 
 for(y=0;y<h;y++)
  for(x=0;x<w;x++) {
   if((x<pwin->ws_conf.ws_col) &&
      (y<pwin->ws_conf.ws_row)) {
    text =   old_chars[x + (y * pwin->ws_conf.ws_col)].text;
    col =    old_chars[x + (y * pwin->ws_conf.ws_col)].col;
    attrib = old_chars[x + (y * pwin->ws_conf.ws_col)].attrib;
   } else {
    text = 0;
    col = 0;
    attrib = 0;
   }
   pwin->chars[x + (y*w)].text =text;
   pwin->chars[x+  (y*w)].col = col;
   pwin->chars[x + (y*w)].attrib= attrib;
  }

 pwin->ws_conf.ws_col = w;
 pwin->ws_conf.ws_row = h;
 free(old_chars);
 if(pwin->cursor_x > w)
  pwin->cursor_x = w;
 if(pwin->cursor_y > h)
  pwin->cursor_y = h;

 pwin->sy1 = 0;
 pwin->sy2 = h-1;
}
