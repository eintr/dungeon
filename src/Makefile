include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE -DINSTALL_PREFIX=\"$(PREFIX)\" 

LDFLAGS+=-lpthread -lm -ldl

sources=ds_bufferlist.c conf.c aa_connection.c ds_hasht.c ds_llist.c ds_memvec.c ds_string.c cJSON.c main.c util_log.c imp_pool.c ds_state_dict.c util_syscall.c thr_maintainer.c thr_monitor.c thr_worker.c module_handler.c gen_context.c

objects=$(sources:.c=.o)


all: aa_proxy

modules:
	make -C modules all

aa_proxy: $(objects)
	$(CC) -o $@ $^ $(LDFLAGS)

install: all modules
	$(INSTALL) aa_proxy ../dist/sbin/

clean:
	rm -f *.o aa_proxy

