#ifndef BUFFERLIST_H
#define BUFFERLIST_H

/** \file ds_bufferlist.h
    A ring list of buffers, for data buffering.
*/

/** \cond */
#include <string.h>
/** \endcond */
#include "ds_llist.h"

/**
    \def DEFAULT_BUFSIZE
        Default buffer limitation in bytes.
*/
#define	DEFAULT_BUFSIZE	65536

/** node of buffer_list_t struct define */
typedef struct {
	void *start;	/**< Buffer memory start address */
	void *pos;		/**< Buffer data start address */
	size_t size;	/**< Buffer data size in bytes */
} buffer_node_t;

/** buffer_list_t struct define */
typedef struct {
	llist_t *base;	/**< A list of buffer_node_t */
	uint32_t bufsize;	/**< */
	uint32_t max;	/**< Max volume of buffer_list in bytes */
	size_t nbytes;	/**< Total byte count in buffer_list */
} buffer_list_t;


/** Create a new buffer.
    \param bufsize Max volume of buffer_list in bytes.
    \return The buffer_list pointer.
*/
buffer_list_t *buffer_new(uint32_t bufsize);

/** Delete a buffer.
    \param p Pointer of the buffer_list.
    \return Return 0 if OK.
*/
int buffer_delete(buffer_list_t *p);

/** Determine how many bytes in a buffer.
    \param p Pointer of the buffer_list.
    \return Total byte count in buffer_list.
*/
size_t buffer_nbytes(buffer_list_t *p);

/** Read data from a buffer.
    \param p Pointer of the buffer_list.
    \param buf Pointer of buffer.
    \param size Size of buffer, write operation never exceeds this bounry.
    \return How many bytes read actually.
*/
ssize_t buffer_read(buffer_list_t *p, void *buf, size_t size);

/** Write data to a buffer.
    \param p Pointer of the buffer_list.
    \param buf Pointer of data to be written.
    \param size Size of data.
    \return How many bytes written actually.
*/
ssize_t buffer_write(buffer_list_t *p, const void *buf, size_t size);

void * buffer_get_next(buffer_list_t *bl, void *ptr);
void * buffer_get_head(buffer_list_t *bl);

void * buffer_get_data(void *buf);

int buffer_pop(buffer_list_t *bl);
int buffer_move_head(buffer_list_t *bl, size_t size);

#endif

