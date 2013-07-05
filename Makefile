include config.mk

all:
%:
	make -C src $@
#	make -C doc $@
	
install:
	[ -d $(PREFIX) ] || mkdir -p $(PREFIX)
	[ -d $(SBINDIR) ] || mkdir $(SBINDIR)
	[ -d $(CONFDIR) ] || mkdir $(CONFDIR)

	make -C src $@
#	make -C doc $@

	$(INSTALL) dist/* $(SBINDIR)
	[ -e $(CONFDIR)/aa_proxy.conf ] || cp doc/aaproxy.conf.example  $(CONFDIR)/aa_proxy.conf

clean:
	make -C src $@
#	make -C doc $@

