#ifndef BUFFERLIST_H
#define BUFFERLIST_H

#include <string.h>
#include "aa_llist.h"


#define	DEFAULT_BUFSIZE	65536

typedef struct {
	void *start;
	void *pos;
	size_t size;
} buffer_node_t;

typedef struct {
	llist_t *base;
	uint32_t bufsize;
	uint32_t max;
	size_t nbytes;
} buffer_list_t;



buffer_list_t *buffer_new(uint32_t bufsize);

int buffer_delete(buffer_list_t*);

size_t buffer_nbytes(buffer_list_t*);

ssize_t buffer_read(buffer_list_t*, void *buf, size_t size);

ssize_t buffer_write(buffer_list_t*, const void *buf, size_t size);

#endif

