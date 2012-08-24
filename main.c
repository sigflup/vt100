#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <X11/Xcms.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <libutil.h>

#include "vt100.h"
#include "async.h"
#include "main.h"

#define WIDTH	80
#define HEIGHT	24
#define FONT	"fixed"

#define BUFFER_LEN	1024

#define EVENT_MASK (ExposureMask|StructureNotifyMask|KeyPressMask)

term_t main_term;
int cursor = 0;

int master, slave;
pid_t child_pid;

struct {
 Display *dpy;
 int pix_w, pix_h;
 int w, h;
 int font_w, font_h;
 int screen;
 Window root;
 Window window;
 GC gc;
 XFontStruct *font;
 unsigned long black_pixel;
 unsigned long white_pixel;
 Colormap colormap;
} window;


char *colormap_txt[] = {
 "#000000",
 "#af0000", 
 "#00af00",
 "#afaf00",

 "#1f1faf",
 "#af00af",
 "#00afaf",
 "#afafaf",

 "#000000",
 "#ff0000",
 "#00ff00",
 "#ffff00",

 "#1f1fff",
 "#ff00ff",
 "#00ffff",
 "#ffffff"
};

XColor colors[16];

void init_colors(void) {
 int i;
 window.colormap = DefaultColormap(window.dpy, 0);
 for(i=0;i<16;i++) {
  XParseColor(window.dpy, window.colormap, colormap_txt[i], &colors[i]);
  XAllocColor(window.dpy, window.colormap, &colors[i]); 
 }
}

void d_char(int x, int y, char_t *c) {
 unsigned char *data;
 int offset;
 int fg, bg;

 bg = c->col&0xf;
 fg = (c->col>>4)&0xf;

 if(bg == 0)
  XSetBackground(window.dpy, window.gc, window.white_pixel);
 else
  XSetBackground(window.dpy, window.gc, colors[bg].pixel);

 if(fg == 7)
  XSetForeground(window.dpy, window.gc, window.black_pixel);
 else
  XSetForeground(window.dpy, window.gc, colors[fg].pixel);

 offset = window.font->max_bounds.ascent;
 if(c->text == 0)
  data = (unsigned char *)" ";
 else
  data = &c->text;
 XDrawImageString(window.dpy,window.window,window.gc,
   x*window.font_w,
   (y*window.font_h)+offset, (char *)data, 1);
}

void d_drawcursor(void) {
 if(cursor == 1) {
  XFillRectangle(window.dpy,window.window,window.gc,
   main_term.cursor_x * window.font_w,
   main_term.cursor_y * window.font_h,
   window.font_w,
   window.font_h);
 } else 
  d_char(main_term.cursor_x, main_term.cursor_y, 
    &main_term.chars[main_term.cursor_x+(window.w*main_term.cursor_y)]);
 

}
void d_drawscreen(void) {
 int x, y;
 int offset;
 offset = window.font->max_bounds.ascent;
 if(main_term.dirty == 1) {
  for(y=0;y<window.h;y++)
   for(x=0;x<window.w;x++) 
    d_char(x,y, &main_term.chars[x+(window.w*y)]);
   
  main_term.dirty = 0;
 }
 d_drawcursor();
 XFlush(window.dpy);
}

void q_input(void) {
 struct winsize wsize;
 XEvent e;
 KeySym key;
 int vector;
 unsigned char byte;
 while(XCheckMaskEvent(window.dpy, EVENT_MASK, &e)) {
  switch(e.type) {
   case Expose:
    main_term.dirty = 1;
    break;
   case ConfigureNotify:
    window.pix_w = e.xconfigure.width;
    window.pix_h = e.xconfigure.height;
    window.w = window.pix_w / window.font_w;
    window.h = window.pix_h / window.font_h;
    wsize.ws_col = window.w;
    wsize.ws_row = window.h;
    ioctl(slave, TIOCSWINSZ, &wsize);
    resize_term((struct term_t *)&main_term, window.w, window.h);
    break;
   case KeyPress:
    vector = 0;
    if((e.xkey.state & ShiftMask) == ShiftMask) vector = 1;
    if((e.xkey.state & LockMask) == LockMask) vector = 1;
    key = XKeycodeToKeysym(window.dpy, e.xkey.keycode, vector);
    byte = (unsigned char)key&0xff;
    if(key < 0xff00) {
     if((e.xkey.state & ControlMask)==ControlMask) {
      byte = toupper(byte);
      if((byte >= '@') && (byte <='_')) {
       byte -= 0x40;
       main_term.char_out(byte, (struct term_t *)&main_term);
      }
     } else
      main_term.char_out(byte, (struct term_t *)&main_term);
    } else {
     switch(key) {
      case XK_Escape:
       main_term.char_out('\33', (struct term_t *)&main_term);
       break;
      case XK_Return:
       main_term.char_out('\r', (struct term_t *)&main_term);
       break;
      case XK_Left:
       vt_send((struct term_t *)&main_term, K_LT);
       break;
      case XK_Right:
        vt_send((struct term_t *)&main_term, K_RT);
	break;
      case XK_Up:
        vt_send((struct term_t *)&main_term, K_UP);
	break;
       case XK_Down:
        vt_send((struct term_t *)&main_term, K_DN);
	break;
       case XK_Page_Up:
        vt_send((struct term_t *)&main_term, K_PGUP);
	break;
       case XK_Page_Down:
        vt_send((struct term_t *)&main_term, K_PGDN);
	break;
       case XK_Home:
        vt_send((struct term_t *)&main_term, K_HOME);
	break;
       case XK_Insert:
        vt_send((struct term_t *)&main_term, K_INS);
	break;
       case XK_Delete:
        vt_send((struct term_t *)&main_term, K_DEL);
	break;
       case XK_End:
        vt_send((struct term_t *)&main_term, K_END);
	break;
       case XK_Tab:
        main_term.char_out(main_term.control_keys[CONTROL_TAB],
	 (struct term_t *)&main_term);
	break;
      case XK_BackSpace:
       main_term.char_out(main_term.control_keys[CONTROL_BACKSPACE],
	 (struct term_t *)&main_term);
       break;
     }
    }


    break;
  }
 }
}

void new_shell(void) {
 struct winsize wsize;
 char slave_name[16];

 if(openpty(&master, &slave, slave_name, NULL, &main_term.ws_conf)<0) {
  perror("openpty");
  exit(1);
 }

 if((child_pid = fork())==0) {
  close(master);
  login_tty(slave);
  execl("/bin/sh", "/bin/sh", (char *)0);
 }
 fcntl(master, F_SETFL, fcntl(master, F_GETFL,0)|O_DIRECT|O_NONBLOCK); 
 wsize.ws_col = window.w;
 wsize.ws_row = window.h;
 ioctl(slave, TIOCSWINSZ, &wsize);
}

void io(void) {
 unsigned char buffer[BUFFER_LEN];
 int len;
 int i;

 while((len = read(master, buffer, BUFFER_LEN))>0) {
  for(i=0;i<len;i++)
   vt_out((struct term_t *)&main_term, buffer[i]);
 }

}

int main(void) {
 struct itimerval value;

 if(!(window.dpy = XOpenDisplay(NULL))) {
  fprintf(stderr, "Could not open display.\n");
  exit(1);
 }

 if(!(window.font = XLoadQueryFont(window.dpy, FONT))) {
  fprintf(stderr, "unable to load font\n");
  exit(1);
 }
 window.font_w=
   window.font->max_bounds.rbearing-window.font->min_bounds.lbearing;
 window.font_h= 
   window.font->max_bounds.ascent + window.font->max_bounds.descent; 


 window.screen = DefaultScreen(window.dpy);
 window.root = RootWindow(window.dpy, window.screen);
 window.black_pixel = BlackPixel(window.dpy, window.screen);
 window.white_pixel=  WhitePixel(window.dpy, window.screen);

 window.w = WIDTH;
 window.h = HEIGHT;

 window.pix_w = window.font_w * window.w; 
 window.pix_h = window.font_h * window.h;
 window.window = XCreateSimpleWindow(window.dpy,
                                     window.root,
				     1,1,
				     window.pix_w, window.pix_h,
				     0,
				     window.black_pixel,
				     window.white_pixel);

 XSelectInput(window.dpy, window.window, EVENT_MASK);
 XMapWindow(window.dpy, window.window);

 window.gc = XCreateGC(window.dpy, window.window,0,0);
 XSetBackground(window.dpy, window.gc, window.white_pixel);
 XSetForeground(window.dpy, window.gc, window.black_pixel);


 XSetFont(window.dpy, window.gc, window.font->fid);

 init_colors();

 init_term((struct term_t *)&main_term, callback, window.w, window.h);
 main_term.control_keys[CONTROL_BACKSPACE] = BACKSPACE;
 main_term.control_keys[CONTROL_QUIT] = QUIT;
 main_term.control_keys[CONTROL_DOWN] = 0xa;
 main_term.control_keys[CONTROL_TAB] = TAB;
 main_term.control_keys[CONTROL_AAA] = 1;

 new_shell();

 value.it_interval.tv_sec=0;
 value.it_interval.tv_usec=40000;
 value.it_value.tv_sec=0;
 value.it_value.tv_usec=40000;
 setitimer(0, &value, NULL);
 signal(SIGALRM, sig_alarm);
 signal(SIGCHLD, sig_child);
 for(;;) {
  q_input();
  io();
  usleep(20);
 }
 return 0;
}
