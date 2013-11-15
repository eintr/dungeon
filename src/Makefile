include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE -DINSTALL_PREFIX=\"$(PREFIX)\" 

LDFLAGS+=-lpthread -lm -ldl

sources=aa_bufferlist.c aa_conf.c aa_connection.c aa_hasht.c aa_http.c aa_llist.c aa_memvec.c aa_string.c aa_url.c cJSON.c main.c aa_log.c proxy_pool.c aa_state_dict.c aa_syscall.c aa_monitor.c aa_module_handler.c gen_context.c

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

