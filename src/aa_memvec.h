#ifndef AA_MEMVEC_H
#define AA_MEMVEC_H

typedef struct {
	uint8_t *ptr;
	int size;
} memvec_t;

memvec_t *memvec_new(const void*, int);

int memvec_delete(memvec_t*);

int memvec_cmp(memvec_t*, memvec_t*);
int memvec_cmp_content(memvec_t*, memvec_t*);

memvec_t *memvec_dup(memvec_t*);

char *memvec_serialize(memvec_t*);

#endif

