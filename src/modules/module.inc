
objects=$(sources:.c=.o)
TARGET=$(MOD_NAME).so
CFLAGS+=-I../../ -DMOD_NAME=\"$(MOD_NAME)\" -g

all: $(TARGET)

$(MOD_NAME).so: $(objects)
	$(CC) -shared -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -fPIC -c -o $@ $^

install:
	cp $(TARGET) ../../../dist/rooms/

clean:
	rm -f *.o $(TARGET)

