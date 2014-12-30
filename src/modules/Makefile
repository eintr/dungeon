include ../../config.mk

module_dirs=fake_httpd

all: prepare room_exports.h
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

room_exports.h:
	> $@ ; \
	for d in $(module_dirs) ; do \
		[ -r $$d/export.h ] && cat $$d/export.h >> $@;	\
	done

prepare:
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

install:
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

clean:
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

