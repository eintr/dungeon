#define HASHT_H
#define HASHT_H

typedef uint32_t hash_func_t(memvec_t*);

typedef void hasht_t;

hasht_t *hasht_new(hash_func_t*, volume);
int hasht_delete(hash_func_t*);

int hasht_add_item(memvec_t*, void*);

int hasht_find_item(memvec_t*, void*);

int hasht_delete_item(memvec_t*, void*);

#endif

