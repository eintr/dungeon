#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include <pthread.h>

#include "aa_hasht.h"

struct node_st {
	hashval_t keyval;
	hashkey_t key;
	void *value;
	int update_count, lookup_count;
	struct timeval create_tv, last_lookup_tv, last_update_tv;
};

struct bucket_st {
	pthread_rwlock_t rwlock;
	struct node_st *node;
	int nr_nodes, bucket_size;
};

struct hasht_st {
	pthread_rwlock_t rwlock;
	hash_func_t *hash_func;
	int volume, nr_nodes;
	struct bucket_st *bucket;
	int nr_buckets;
};

static void *get_key_ptr(hashkey_t *, void *);
static void *get_nodekey_ptr(struct node_st*);
static int get_nodekey_len(struct node_st *);

#define min(x,y) (x)<(y)?(x):(y)

static int bucket_get_free_pos(struct bucket_st *b)
{
	int i, ret;
	void *tmp;

	for (i=0;i<b->bucket_size;++i) {
		if (b->node[i].value==NULL) {
			return i;
		}
	}
	tmp = realloc(b->node, sizeof(struct node_st)*(b->bucket_size+b->bucket_size>>2+1));
	if (tmp==NULL) {
		return -1;
	}
	ret = b->bucket_size;
	b->bucket_size += b->bucket_size>>2+1;
	for (i=ret;i<b->bucket_size;++i) {
		b->node[i].value = NULL;
	}
	return ret;
}

static hashval_t default_hash_func(memvec_t *v) {
	register int i;
	register hashval_t res=0xffffffff;

	for (i=0;i<v->size;++i) {
		res ^= v->ptr[i];
	}

	return res;
}

hasht_t *hasht_new(hash_func_t *func, int volume)
{
	register int i;
	struct hasht_st *newnode;

	if (func==NULL) {
		func=default_hash_func;
	}
	if (volume<0) {
		return NULL;
	}

	newnode = malloc(sizeof(*newnode));
	if (newnode==NULL) {
	}

	pthread_rwlock_init(&newnode->rwlock, NULL);
	newnode->volume = volume;
	newnode->nr_nodes = 0;
	newnode->nr_buckets = volume>>3;
	newnode->hash_func = func;
	newnode->bucket = malloc(sizeof(struct node_st)*newnode->nr_buckets);
	if (newnode->bucket == NULL) {
	}
	for (i=0;i<newnode->nr_buckets;++i) {
		newnode->bucket[i].node = NULL;
		newnode->bucket[i].nr_nodes = 0;
		pthread_rwlock_init(&newnode->bucket[i].rwlock, NULL);
	}

	return newnode;
}

int hasht_delete(hasht_t *p)
{
	register int i;
	struct hasht_st *self=p;

	pthread_rwlock_destroy(&self->rwlock);
	for (i=0;i<self->nr_buckets;++i) {
		pthread_rwlock_destroy(&self->bucket[i].rwlock);
		free(self->bucket[i].node);
	}
	free(self->bucket);
	free(self);
	return 0;
}

int hasht_add_item(hasht_t *p, hashkey_t *key, void *data)
{
	struct hasht_st *self=p;
	hashval_t hash;
	int pos;
	memvec_t tmp;

	if (self->nr_nodes >= self->volume) {
		return ENOMEM;
	}

	tmp.ptr = get_key_ptr(key, data);
	tmp.size = key->len;
	hash = self->hash_func(&tmp);

	pthread_rwlock_wrlock(&self->rwlock);
	self->nr_nodes++;
	pthread_rwlock_unlock(&self->rwlock);

	pthread_rwlock_wrlock(&self->bucket[hash].rwlock);
	pos = bucket_get_free_pos(&self->bucket[hash]);

	self->bucket[hash].node[pos].keyval = hash;
	self->bucket[hash].node[pos].key.offset = key->offset;
	self->bucket[hash].node[pos].key.len = key->len;
	self->bucket[hash].node[pos].value = data;
	self->bucket[hash].node[pos].update_count = 0;
	gettimeofday(&self->bucket[hash].node[pos].create_tv, NULL);
	self->bucket[hash].node[pos].last_lookup_tv.tv_sec = 0;
	self->bucket[hash].node[pos].last_lookup_tv.tv_usec = 0;
	self->bucket[hash].node[pos].last_update_tv.tv_sec = 0;
	self->bucket[hash].node[pos].last_update_tv.tv_usec = 0;
	pthread_rwlock_unlock(&self->bucket[hash].rwlock);
	return 0;
}

static void *get_key_ptr(hashkey_t *key, void *data)
{
	char *tmp=data;
	return tmp+key->offset;
}

static void *get_nodekey_ptr(struct node_st *node)
{
	char *tmp=node->value;
	return tmp+(node->key.offset);
}
static int get_nodekey_len(struct node_st *node)
{
	return node->key.len;
}

static struct node_st *hasht_find_node(hasht_t *p, hashkey_t *key, void *data)
{
	struct hasht_st *self = p;
	register int i;
	hashval_t hash;
	memvec_t key_vec;

	key_vec.ptr = get_key_ptr(key, data);
	key_vec.size = key->len;
	hash = self->hash_func(&key_vec);

	for (i=0;i<self->bucket[hash].bucket_size;++i) {
		if (self->bucket[hash].node[i].value) {
			if (0==memcmp(
						get_nodekey_ptr((self->bucket[hash].node)+i), 
						key_vec.ptr, 
						min(key_vec.size, get_nodekey_len(self->bucket[hash].node+i))
						)
					) {
				gettimeofday(&self->bucket[hash].node[i].last_lookup_tv);
				self->bucket[hash].node[i].lookup_count++;
				return &self->bucket[hash].node[i];
			}
		}
	}
	return NULL;
}

void *hasht_find_item(hasht_t *p, hashkey_t *key, void *data)
{
	struct node_st *res;
	struct hasht_st *self = p;

	pthread_rwlock_rdlock(&self->rwlock);
	res = hasht_find_node(self, key, data);
	if (res==NULL) {
		pthread_rwlock_unlock(&self->rwlock);
		return NULL;
	} else {
		pthread_rwlock_unlock(&self->rwlock);
		return res->value;
	}
}

int hasht_delete_item(hasht_t *p, hashkey_t *key, void *data)
{
	struct hasht_st *self = p;
	struct node_st *node;

	pthread_rwlock_wrlock(&self->rwlock);
	node = hasht_find_node(self, key, data);
	if (node==NULL) {
		pthread_rwlock_unlock(&self->rwlock);
		return ENOENT;
	} else {
		node->value = NULL;
		self->nr_nodes--;
		pthread_rwlock_unlock(&self->rwlock);
		return 0;
	}
}

