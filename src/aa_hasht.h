#ifndef HASHT_H
#define HASHT_H

#include <stdint.h>

#include "aa_memvec.h"

typedef uint32_t hashval_t;
typedef struct {
	int offset;
	int len;
} hashkey_t;
typedef uint32_t hash_func_t(memvec_t*);

typedef void hasht_t;

#define BUCKET_DEFAULT_SIZE 100

hasht_t *hasht_new(hash_func_t*, int);
int hasht_delete(hasht_t*);

int hasht_add_item(hasht_t*, hashkey_t*, void*);

void *hasht_find_item(hasht_t*, hashkey_t*, void*);

int hasht_delete_item(hasht_t*, hashkey_t*, void*);

#endif

