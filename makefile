PROGS = capture_raw_frames dump telnet

all: $(PROGS)

CFLAGS = -O2 -g -Wall -std=c99

capture_raw_frames pgm2txt: LDFLAGS = -laa

%: %.c fname.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

pgm2txt: pgm2txt.c pgm.c pgm.h
	$(CC) $(CFLAGS) -o $@ $< pgm.c $(LDFLAGS)

clean::
	rm -f $(PROGS) pgm2txt
