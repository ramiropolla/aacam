PROGS = capture_raw_frames dump telnet

all: $(PROGS)

CFLAGS = -O2 -g -Wall -std=c99
LDFLAGS = -laa

%: %.c fname.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean::
	rm -f $(PROGS)
