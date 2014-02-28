include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE

LDFLAGS+=-lpthread -lm -ldl -rdynamic

SERVERFNAME=$(APPNAME)

sources=ds_bufferlist.c conf.c aa_connection.c ds_hasht.c ds_llist.c ds_memvec.c ds_string.c cJSON.c main.c util_log.c dungeon.c ds_state_dict.c util_syscall.c thr_maintainer.c thr_monitor.c thr_worker.c module_handler.c imp_body.c room_service.c imp.c

objects=$(sources:.c=.o)


all: $(SERVERFNAME) rooms

rooms:
	make -C modules all

$(SERVERFNAME): $(objects)
	$(CC) -o $@ $^ $(LDFLAGS)

install: all modules
	$(INSTALL) $(SERVERFNAME) ../dist/sbin/
	make -C modules $@

clean:
	rm -f $(objects) $(SERVERFNAME)
	make -C modules $@

