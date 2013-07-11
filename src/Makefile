include ../config.mk

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE -DINSTALL_PREFIX=\"$(PREFIX)\"

LDFLAGS+=-lpthread -lm 

sources=aa_bufferlist.c aa_conf.c aa_connection.c aa_hasht.c aa_http.c aa_llist.c aa_memvec.c aa_string.c aa_url.c cJSON.c main.c aa_log.c proxy_context.c proxy_pool.c aa_state_dict.c aa_syscall.c

objects=$(sources:.c=.o)


all: aa_proxy
	
aa_proxy: $(objects)
	$(CC) -o $@ $^ $(LDFLAGS)

install:
	$(INSTALL) aa_proxy ../dist/sbin/

clean:
	rm -f *.o aa_proxy

