include config.mk

all:
%:
	make -C src $@

document:
	make -C doc doc
	
install:
	[ -d $(PREFIX) ] || mkdir -p $(PREFIX)
	[ -d $(SBINDIR) ] || mkdir $(SBINDIR)
	[ -d $(CONFDIR) ] || mkdir $(CONFDIR)
	[ -d $(MODDIR) ] || mkdir $(MODDIR)

	make -C src $@
	make -C doc $@

	$(INSTALL) dist/sbin/* $(SBINDIR)/
	$(INSTALL) dist/rooms/* $(MODDIR)/
	[ -e $(CONFDIR)/$(APPNAME).conf ] || cp dist/conf/$(APPNAME).conf  $(CONFDIR)/

clean:
	make -C src $@
	make -C doc $@
	rm -rf dist/sbin/*

