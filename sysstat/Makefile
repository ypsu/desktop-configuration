CFLAGS=-I/usr/include/freetype2
LDFLAGS=-lXft -lXext -lX11 -lasound

all:
	gcc -g -lm -Wall $(CFLAGS) -o sysstat2 sysstat2.c -lasound
	gcc -g -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

prof:
	gcc -p -g -lm -Wall $(CFLAGS) -o sysstat2 sysstat2.c -lasound
	gcc -p -g -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

opt:
	gcc -O3 -march=native -lm -Wall $(CFLAGS) -o sysstat2 sysstat2.c -lasound
	gcc -O3 -march=native -lm -Wall $(CFLAGS) -o sysstat sysstat.c gui.c $(LDFLAGS)

install:
	install sysstat2 ~/.bin/sysstat2
	install sysstat ~/.bin/sysstat
