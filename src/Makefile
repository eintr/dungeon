
PREFIX="/usr/home/guanwei2/test_env/aa_proxy"
SBINDIR=$(PREFIX)/sbin
CONFDIR=$(PREFIX)/conf
LOGDIR=$(PREFIX)/log

INSTALL=install

CFLAGS+=-I. -pthread -Wall -g -D_GNU_SOURCE

LDFLAGS+=-lpthread -lm 

sources=aa_bufferlist.c aa_conf.c aa_connection.c aa_hasht.c aa_http.c aa_llist.c aa_memvec.c aa_string.c aa_url.c cJSON.c main.c aa_log.c proxy_context.c proxy_pool.c

objects=$(sources:.c=.o)


all: aa_proxy
	
aa_proxy: $(objects)
	$(CC) -o $@ $^ $(LDFLAGS)

install:
	[ -d $(PREFIX) ] || mkdir -p $(PREFIX)
	[ -d $(SBINDIR) ] || mkdir $(SBINDIR)
	[ -d $(CONFDIR) ] || mkdir $(CONFDIR)

	$(INSTALL) aa_proxy $(SBINDIR)/
	[ -e $(CONFDIR)/aa_proxy.conf ] || cp ../doc/aaproxy.conf.example  $(CONFDIR)/aa_proxy.conf


clean:
	rm -f *.o aa_proxy








