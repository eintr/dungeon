
objects=$(sources:.c=.o)
TARGETS=$(MOD_NAME).so
CFLAGS+=-I../../ -DMOD_NAME=\"$(MOD_NAME)\"

all: $(TARGETS)

$(MOD_NAME).so: $(objects)
	$(CC) $(LDFLAGS) -shared -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $^

install:
	cp $(TARGETS) ../../../dist/rooms/

clean:
	rm -f *.o $(TARGETS)

