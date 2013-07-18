#ifndef AA_BITMAP_H
#define AA_BITMAP_H

#define	BMAP_MAXBITS	(1024*1024*1024)

typedef struct bitmap_st {
	size_t size, bitsize;
	char data[0];
} bitmap_t;

bitmap_t *bitmap_new(size_t bitsize);

int bitmap_delete(bitmap_t*);

//void bitmap_set(bitmap_t*, size_t);
#define bitmap_set(my,n) ((my)->data[(n)/8]|=1<<((n)%8))

//void bitmap_unset(bitmap_t*, size_t);
#define bitmap_unset(my,n) ((my)->data[(n)/8] &= ~(1<<((n)%8)))

//int bitmap_isset(bitmap_t*, size_t);
#define bitmap_isset(my,n) ((my)->data[(n)/8]&1<<((n)%8))

#endif

