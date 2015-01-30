include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE -Wall

LDFLAGS+=-lpthread -lm -ldl -rdynamic

SERVERFNAME=$(APPNAME)

sources=ds_olist.c ds_queue2.c ds_bufferlist.c conf.c util_conn_tcp.c ds_hasht.c ds_llist.c ds_memvec.c cJSON.c main.c util_log.c dungeon.c util_syscall.c thr_gravekeeper.c thr_monitor.c thr_ioevent.c thr_worker.c module_handler.c room_service.c imp.c util_mutex.c

objects=$(sources:.c=.o)


all: room_exports.h $(SERVERFNAME) rooms

room_exports.h:
	make -C modules room_exports.h

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

