################ Edit lines below #################

PREFIX=/usr/local/dungeon
SBINDIR=$(PREFIX)/sbin
CONFDIR=$(PREFIX)/conf
LOGDIR=$(PREFIX)/log
MODDIR=$(PREFIX)/modules

################ DON'T Edit lines below #################

APPNAME=dungeon
APPVERSION_MAJ=0
APPVERSION_MIN=5

INSTALL=install

CFLAGS+=-DAPPNAME=\"$(APPNAME)\" -DAPPVERSION_MAJ=\"$(APPVERSION_MAJ)\" -DAPPVERSION_MIN=\"$(APPVERSION_MIN)\" -DCONFDIR=\"$(CONFDIR)\" -DINSTALL_PREFIX=\"$(PREFIX)\" -DMODDIR=\"$(MODDIR)\"

