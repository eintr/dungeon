include ../config.mk

all: doxygen
	echo

doxygen:
	doxygen doxy.conf

install:
	cp conf.example ../dist/conf/$(APPNAME).conf

clean:
	rm -f ../dist/conf/$(APPNAME).conf

