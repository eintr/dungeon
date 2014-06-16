include ../../config.mk

module_dirs=echo

all: room_exports.h
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

room_exports.h:
	> $@
	for d in $(module_dirs) ; do \
		[ -r $$d/export.h ] && cat $$d/export.h >> $@;	\
	done

install:
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done

clean:
	for d in $(module_dirs) ; do \
		make -C $$d $@;	\
	done
	rm -f room_exports.h

