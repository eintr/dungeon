include ../module.inc

sources=mod.c

TARGETS=mod.so

all: $(TARGETS)

mod.so:	mod.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $^

