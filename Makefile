include config.mk

all:
%:
	make -C src $@
	make -C doc $@
	
install:
	[ -d $(PREFIX) ] || mkdir -p $(PREFIX)
	[ -d $(SBINDIR) ] || mkdir $(SBINDIR)
	[ -d $(CONFDIR) ] || mkdir $(CONFDIR)

	make -C src $@
	make -C doc $@

	$(INSTALL) dist/sbin/* $(SBINDIR)/
	[ -e $(CONFDIR)/aa_proxy.conf ] || cp dist/conf/aa_proxy.conf  $(CONFDIR)/

clean:
	make -C src $@
	make -C doc $@

