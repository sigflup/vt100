CC=gcc
CFLAGS=-I/usr/X11R6/include
LDFLAGS=-L/usr/X11R6/lib -Wall -lX11 -lutil -ggdb

OBJS=main.o vt100.o async.o

vt100: ${OBJS}
	${CC} -o vt100 ${OBJS} ${LDFLAGS}

%.o: %.c
	${CC} -c ${CFLAGS} -o $@ $<

clean:
	rm -f vt100 vt100.core ${OBJS}
