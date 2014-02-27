include ../module.inc

sources=mod.c

TARGETS=dumb.so

all: $(TARGETS)

dumb.so:	mod.o
	$(CC) $(LDFLAGS) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $^

install:
	cp $(TARGETS) ../../../dist/rooms/

clean:
	rm -f *.o $(TARGETS)

