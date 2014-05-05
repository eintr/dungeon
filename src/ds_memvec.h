#ifndef AA_MEMVEC_H
#define AA_MEMVEC_H

/** \cond 0 */
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
/** \endcond */

typedef struct {
	uint8_t *ptr;
	size_t size;
} memvec_t;

memvec_t *memvec_new(const void*, size_t);

int memvec_delete(memvec_t*);

int memvec_cmp(memvec_t*, memvec_t*);
int memvec_cmp_content(memvec_t*, memvec_t*);

memvec_t *memvec_dup(memvec_t*);

char *memvec_serialize(memvec_t*);

#endif

