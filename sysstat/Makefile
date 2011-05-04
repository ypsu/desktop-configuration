CFLAGS=-I/usr/include/freetype2
LDFLAGS=-lXft -lXext -lasound

all:
	gcc -g -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

prof:
	gcc -p -g -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

opt:
	gcc -O3 -march=native -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

install:
	cp sysstat ~/.xmonad/sysstat
