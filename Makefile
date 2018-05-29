CFLAGS = -Wall -ggdb -g -O0 -D_GNU_SOURCE
LDFLAGS = -lm -lcurses

all: lurker

wav.o: wav.c wav.h riff.c riff.h
lurker.o: lurker.c wav.c wav.h riff.c riff.h
riff.o: riff.c riff.h

clean:
	rm -f *.o lurker

lurker: lurker.o riff.o wav.o

