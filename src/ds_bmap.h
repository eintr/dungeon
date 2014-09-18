#ifndef DS_BITMAP_H
#define DS_BITMAP_H

/** \file ds_bmap.h
		A fast bit map. The bit map size is now hardly limited to 1G.
*/

/**
	\def BMAP_MAXBITS
		Bitmap maximum bit size limitation.
*/
#define	BMAP_MAXBITS	(1024*1024*1024)

/** Bitmap struct */
typedef struct bitmap_st {
	size_t	size;		/**< Bit map size in bytes */
	size_t	bitsize;	/**< Bit map bit size actually in use */
	uint64_t	data[0];	/**< Bit map dynamic memory address */
} bitmap_t;

/** Initiate an all zero bitmap.
	\param bitsize Initial bit size of the bitmap.
	\return The bitmap pointer.
*/
bitmap_t *bitmap_new(size_t bitsize);

/** Delete a bitmap
	\return Return 0 if OK.
*/
int bitmap_delete(bitmap_t*);

/** Set the nth bit to 1 */
#define bitmap_set(my,n) ((my)->data[(n)/64]|=1<<((n)%64))

/** Set the nth bit to 0 */
#define bitmap_unset(my,n) ((my)->data[(n)/64] &= ~(1<<((n)%64)))

/** Test the nth bit */
#define bitmap_isset(my,n) ((my)->data[(n)/64]&1<<((n)%64))

#endif

