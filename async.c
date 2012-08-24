#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <signal.h>
#include <sys/time.h>

#include "vt100.h"
#include "async.h"
#include "main.h"


int timer = 0;

void sig_child(int sig) {
 exit(1);
}

void sig_alarm(int sig) {
 timer++;
 if((timer % 10) == 0) 
  cursor^=1;
 d_drawscreen();
}

void callback(char c, struct term_t *win) {
 cursor = 1;
 write(master, &c, 1);
}
