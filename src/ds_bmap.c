/**
	Bit map implementation.
*/
/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/** \endcond */
#include "ds_bmap.h"

bitmap_t *bitmap_new(size_t bitsize)
{
	bitmap_t *new;

	if (bitsize>BMAP_MAXBITS) {
		return NULL;
	}

	bitsize = ((bitsize%64)?bitsize/64+1:bitsize/64)*64;

	new = malloc(sizeof(struct bitmap_st)+bitsize/8);
	if (new==NULL) {
		return NULL;
	}

	new->bitsize = bitsize;
	new->size = bitsize/8;

	memset(new->data, 0, new->size);

	return new;
}

int bitmap_delete(bitmap_t *p)
{
	free(p);
	return 0;
}

