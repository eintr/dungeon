include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE -DINSTALL_PREFIX=\"$(PREFIX)\" 

LDFLAGS+=-lpthread -lm -ldl

sources=ds_bufferlist.c aa_conf.c aa_connection.c ds_hasht.c aa_http.c ds_llist.c ds_memvec.c ds_string.c aa_url.c cJSON.c main.c util_log.c proxy_pool.c aa_state_dict.c util_syscall.c aa_monitor.c aa_module_handler.c gen_context.c

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

