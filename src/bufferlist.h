#ifndef BUFFER_LIST_H
#define BUFFER_LIST_H

#define	DEFAULT_BUFSIZE	65536

typedef void buffer_t;

buffer_t *buffer_new(int bufsize);

int buffer_delete(buffer_t*);

ssize_t buffer_nbytes(buffer_t*);

ssize_t buffer_read_nb(buffer_t*, void *buf, size_t size);

ssize_t buffer_write_nb(buffer_t*, const void *buf, size_t size);

#endif

