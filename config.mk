################ Edit lines below #################

PREFIX=/usr/local/aa_proxy
SBINDIR=$(PREFIX)/sbin
CONFDIR=$(PREFIX)/conf
LOGDIR=$(PREFIX)/log

################ DON'T Edit lines below #################

INSTALL=install

CFLAGS+=-DCONFDIR=\"$(CONFDIR)\"

