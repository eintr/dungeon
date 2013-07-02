#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/time.h>

#include "aa_hasht.h"
#include "aa_log.h"

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
	int lookup_count, hit_count;
};

static void *get_key_ptr(const hashkey_t *, void *);
static void *get_nodekey_ptr(struct node_st*);
static int get_nodekey_len(struct node_st *);

#define min(x,y) (x)<(y)?(x):(y)

static struct node_st * bucket_new_bucket(int size) 
{
	struct node_st *n;
	int i;

	if (size <= 0) {
		return NULL;
	}

	n = malloc(sizeof(struct node_st) * size);
	if (n == NULL) {
		return NULL;
	}

	for (i = 0; i < size; i++) {
		n[i].value = NULL;
	}

	return n;
}

static int bucket_get_free_pos(struct bucket_st *b)
{
	int i, ret;
	void *tmp;

	if (b->node == NULL) {
		b->node = bucket_new_bucket(BUCKET_DEFAULT_SIZE);
		if (b->node) {
			b->bucket_size = BUCKET_DEFAULT_SIZE;
			return 0;
		}
		return -1;
	}

	for (i = 0; i < b->bucket_size; ++i) {
		if (b->node[i].value == NULL) {
			return i;
		}
	}
	tmp = realloc(b->node, sizeof(struct node_st) * (b->bucket_size + (b->bucket_size >> 2) + 1));
	if (tmp == NULL) {
		return -1;
	}

	b->node = tmp;
	ret = b->bucket_size;
	b->bucket_size += (b->bucket_size >> 2) + 1;
	for (i = ret; i < b->bucket_size; ++i) {
		b->node[i].value = NULL;
	}
	return ret;
}

static hashval_t default_hash_func(memvec_t *v) {
	register uint32_t c1=0xcc9e2d51;
	register uint32_t c2=0x1b873593;
	register uint32_t r1=15, r2=13;
	register uint32_t m=5, n=0xe6546b64;
	register uint32_t k, i, tmp;
	register uint32_t hash;
	union {
		uint32_t u32;
		uint8_t u8[4];
	} *ptr;
	unsigned char *data = v->ptr;
	size_t len = v->size;

	hash = 20000033;
	ptr = (void*)data;
	for (i=0;i<len/4;++i) {
		k = ptr[i].u32;
		k *= c1;
		k = (k<<r1) | (k>>(32-r1));
		k *= c2;

		hash ^= k;
		hash = (hash<<r2) | (hash>>(32-r2));
		hash = hash*m+n;
	}
	tmp = 0;
	if (len%4==3) {
		tmp = ptr->u8[0] + (ptr->u8[1]<<8) + (ptr->u8[2]<<16);
	} else if (len%4==2) {
		tmp = ptr->u8[0] + (ptr->u8[1]<<8);
	} else if (len%4==1) {
		tmp = ptr->u8[0];
	}
	tmp = (tmp>>16) | (tmp<<16);
	hash ^= tmp;
	hash ^= len;

	hash ^= hash>>16;
	hash *= 0x85ebca6b;
	hash ^= hash>>13;
	hash *= 0xc2b2ae35;
	hash ^= hash>>16;

	return hash;
}

hasht_t *hasht_new(hash_func_t *func, int volume)
{
	register int i;
	struct hasht_st *newnode;

	if (func==NULL) {
		func=default_hash_func;
	}
	if (volume <= 0) {
		return NULL;
	}

	newnode = malloc(sizeof(*newnode));
	if (newnode==NULL) {
		return NULL;
	}

	pthread_rwlock_init(&newnode->rwlock, NULL);
	newnode->volume = volume;
	newnode->nr_nodes = 0;
	newnode->nr_buckets = volume>>3;
	newnode->lookup_count = 0;
	newnode->hit_count = 0;
	if (newnode->nr_buckets == 0) {
		newnode->nr_buckets = 1;
	}
	newnode->hash_func = func;
	newnode->bucket = malloc(sizeof(struct node_st)*newnode->nr_buckets);
	if (newnode->bucket == NULL) {
		pthread_rwlock_destroy(&newnode->rwlock);
		free(newnode);
		return NULL;
	}
	for (i=0;i<newnode->nr_buckets;++i) {
		newnode->bucket[i].node = NULL;
		newnode->bucket[i].nr_nodes = 0;
		newnode->bucket[i].bucket_size = 0;
		pthread_rwlock_init(&newnode->bucket[i].rwlock, NULL);
	}

	return newnode;
}

int hasht_delete(hasht_t *h)
{
	register int i;
	struct hasht_st *self=h;

	pthread_rwlock_destroy(&self->rwlock);
	for (i=0;i<self->nr_buckets;++i) {
		pthread_rwlock_destroy(&self->bucket[i].rwlock);
		free(self->bucket[i].node);
	}
	free(self->bucket);
	free(self);
	return 0;
}

int hasht_add_item(hasht_t *h, const hashkey_t *key, void *data)
{
	struct hasht_st *self=h;
	hashval_t hash;
	int pos;
	memvec_t tmp;

	if (self->nr_nodes >= self->volume) {
		return ENOMEM;
	}

	tmp.ptr = get_key_ptr(key, data);
	tmp.size = key->len;
	hash = self->hash_func(&tmp) % self->nr_buckets;

	pthread_rwlock_wrlock(&self->bucket[hash].rwlock);
	pos = bucket_get_free_pos(&self->bucket[hash]);
	if (pos == -1) {
		printf("get free pos failed\n");
		pthread_rwlock_unlock(&self->bucket[hash].rwlock);
		return ENOMEM;
	}

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
	self->bucket[hash].nr_nodes++;
	pthread_rwlock_unlock(&self->bucket[hash].rwlock);
	
	pthread_rwlock_wrlock(&self->rwlock);
	self->nr_nodes++;
	pthread_rwlock_unlock(&self->rwlock);
	
	return 0;
}

static void *get_key_ptr(const hashkey_t *key, void *data)
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

static struct node_st *hasht_find_node(hasht_t *h, const hashkey_t *key, void *data)
{
	struct hasht_st *self = h;
	register int i;
	hashval_t hash;
	memvec_t key_vec;

	self->lookup_count++;
	key_vec.ptr = get_key_ptr(key, data);
	key_vec.size = key->len;
	hash = self->hash_func(&key_vec) % self->nr_buckets;

	for (i=0;i<self->bucket[hash].bucket_size;++i) {
		if (self->bucket[hash].node && self->bucket[hash].node[i].value) {
			if (0==memcmp(
						get_nodekey_ptr((self->bucket[hash].node)+i), 
						key_vec.ptr, 
						min(key_vec.size, get_nodekey_len(self->bucket[hash].node+i))
						)
			   ) {
				gettimeofday(&self->bucket[hash].node[i].last_lookup_tv, NULL);
				self->bucket[hash].node[i].lookup_count++;
				self->hit_count++;
				return &self->bucket[hash].node[i];
			}
		}
	}
	return NULL;
}

void *hasht_find_item(hasht_t *h, const hashkey_t *key, void *data)
{
	struct node_st *res;
	struct hasht_st *self = h;
	hashval_t hash;
	memvec_t key_vec;
	
	key_vec.ptr = get_key_ptr(key, data);
	key_vec.size = key->len;
	hash = self->hash_func(&key_vec) % self->nr_buckets;

	pthread_rwlock_rdlock(&self->bucket[hash].rwlock);
	res = hasht_find_node(self, key, data);
	if (res == NULL) {
		pthread_rwlock_unlock(&self->bucket[hash].rwlock);
		return NULL;
	} else {
		pthread_rwlock_unlock(&self->bucket[hash].rwlock);
		return res->value;
	}
}

int hasht_delete_item(hasht_t *h, const hashkey_t *key, void *data)
{
	struct hasht_st *self = h;
	struct node_st *node;
	hashval_t hash;
	memvec_t key_vec;
	
	key_vec.ptr = get_key_ptr(key, data);
	key_vec.size = key->len;
	hash = self->hash_func(&key_vec) % self->nr_buckets;

	pthread_rwlock_wrlock(&self->bucket[hash].rwlock);
	node = hasht_find_node(self, key, data);
	if (node==NULL) {
		pthread_rwlock_unlock(&self->bucket[hash].rwlock);
		return ENOENT;
	} 

	node->value = NULL;
	self->bucket[hash].nr_nodes--;
	pthread_rwlock_unlock(&self->bucket[hash].rwlock);

	pthread_rwlock_wrlock(&self->rwlock);
	self->nr_nodes--;
	pthread_rwlock_unlock(&self->rwlock);

	return 0;
}

int hasht_clean_table(hasht_t *h)
{
	struct hasht_st *self = h;
	int num = 0, i, j;
	
	for (i = 0; i < self->nr_buckets; ++i) {
		pthread_rwlock_wrlock(&self->bucket[i].rwlock);
		for (j = 0; j < self->bucket[i].bucket_size; ++j) {
			if (self->bucket[i].node && self->bucket[i].node[j].value) {
				free(self->bucket[i].node[j].value);
				self->bucket[i].node[j].value = NULL;
				++num;
			}
		}
		self->bucket[i].nr_nodes = 0;
			
		pthread_rwlock_unlock(&self->bucket[i].rwlock);
		
		pthread_rwlock_wrlock(&self->rwlock);
		self->nr_nodes -= num;
		pthread_rwlock_unlock(&self->rwlock);
		num = 0;
	}

	return 0;
}

int hasht_modify_item(hasht_t *h, const hashkey_t *key, void *data, hasht_modify_cb_pt modify, void *args)
{
	struct node_st *res;
	struct hasht_st *self = h;
	hashval_t hash;
	memvec_t key_vec;
	
	key_vec.ptr = get_key_ptr(key, data);
	key_vec.size = key->len;
	hash = self->hash_func(&key_vec) % self->nr_buckets;

	pthread_rwlock_rdlock(&self->bucket[hash].rwlock);
	res = hasht_find_node(self, key, data);
	if (res == NULL) {
		pthread_rwlock_unlock(&self->bucket[hash].rwlock);
		return -1;
	}

	modify(res->value, args);
	pthread_rwlock_unlock(&self->bucket[hash].rwlock);

	return 0;	
}

cJSON *hasht_info_cjson(hasht_t *ptr)
{
	struct hasht_st *p=ptr;
	cJSON *res, *buckets;
	int i;

	res = cJSON_CreateObject();
	cJSON_AddNumberToObject(res, "Volume", p->volume);
	cJSON_AddNumberToObject(res, "Used", p->nr_nodes);
	cJSON_AddNumberToObject(res, "Rate", (double)p->nr_nodes/(double)p->volume);
	cJSON_AddNumberToObject(res, "LookupCount", p->lookup_count);
	cJSON_AddNumberToObject(res, "LookupHit", p->hit_count);

	buckets = cJSON_CreateArray();
	for (i=0;i<p->nr_buckets;++i) {
	}

	cJSON_AddItemToObject(res, "Buckets", buckets);

	return res;
}

#ifdef AA_HASH_TEST

#include "aa_test.h"

typedef struct {
	int num;
	char *str;
} test_data;

struct body_data {
	test_data *data;
	struct hasht_st *ht;
};

const hashkey_t hashkey = {
	(int)&(((test_data *)0)->num),
	sizeof(int)
};

void test_modify(void *item, void *args)
{
	test_data *td;
	char *str;

	td = (test_data *)item;
	str = (char *)args;

	td->str = str;
}

int func_test()
{
	int ret;
	test_data *tp;

	test_data data[3] = {
		{0, "data0"},
		{1, "data1"},
		{2, "data2"}
	};

	struct hasht_st *hashtable = hasht_new(NULL, 10000);

	if (hashtable == NULL) {
		return -1;
	}
	
	/*
	 *	test 1 : basic test
	 */
	ret = hasht_add_item(hashtable, &hashkey, &data[0]);
	if (ret) {
		printf("hasht_add_item failed\n");
		hasht_delete(hashtable);
		return -1;
	}
	ret = hasht_add_item(hashtable, &hashkey, &data[2]);
	if (ret) {
		printf("hasht_add_item failed\n");
		hasht_delete(hashtable);
		return -1;
	}

	int i;
	for (i = 0; i < 3; i++) {
		tp = hasht_find_item(hashtable, &hashkey, &data[i]);
		if (tp == NULL) {
			printf("not find item %d\n", i);
		} else {
			printf("found item %d : str is %s\n", i, tp->str);
		}
	}

	ret = hasht_delete_item(hashtable, &hashkey, &data[0]);
	if (ret) {
		printf("delete item %d failed\n", 0);
	} else {
		printf("delete item %d done\n", 0);
	}

	for (i = 0; i < 3; i++) {
		tp = hasht_find_item(hashtable, &hashkey, &data[i]);
		if (tp == NULL) {
			printf("not find item %d\n", i);
		} else {
			printf("found item %d :  str is %s\n", i, tp->str);
		}
	}

	char *str = "this is new string";
	ret = hasht_modify_item(hashtable, &hashkey, &data[2], test_modify, str);
	if (ret == 0) {
		printf("modify item[2]->str to %s done.\n", str);
	} else {
		printf("modify failed\n");
	}
	for (i = 0; i < 3; i++) {
		tp = hasht_find_item(hashtable, &hashkey, &data[i]);
		if (tp == NULL) {
			printf("not find item %d\n", i);
		} else {
			printf("found item %d : num: %d, str: %s\n", i, tp->num, tp->str);
		}
	}

	hasht_delete(hashtable);
	printf("%stest1 successful%s\n", COR_BEGIN, COR_END);

	return 0;
}

void * adder(void *args)
{
	int l=0;
	int num = 0;
	struct body_data *bd = (struct body_data *)args;
	struct hasht_st *ht  = bd->ht;

	test_data *data = bd->data;

	while(l++<10000) {
		if (hasht_add_item(ht, &hashkey, data) == 0)
			num++;
		usleep(1);
	}
	printf("add success %d\n", num);

	return NULL;
}

void * deleter(void *args)
{
	int l=0;
	int num = 0;
	struct body_data *bd = (struct body_data *)args;
	struct hasht_st *ht  = bd->ht;

	test_data *data = bd->data;

	while(l++<10000) {
		if (hasht_delete_item(ht, &hashkey, data) == 0)
			num++;
		usleep(1);
	}
	printf("delete success %d\n", num);

	return NULL;
}

void * finder(void *args)
{
	int l=0;
	int num = 0;
	struct body_data *bd = (struct body_data *)args;
	struct hasht_st *ht  = bd->ht;

	test_data *data = bd->data;

	while(l++<10000) {
		if (hasht_find_item(ht, &hashkey, data) != NULL)
			num++;
		usleep(1);
	}
	printf("find success %d\n", num);

	return NULL;
}

void multi_thread_test()
{
	test_data data = {11, "data1"};

	struct hasht_st *hashtable = hasht_new(NULL, 1000);

	if (hashtable == NULL) {
		return;
	}

	struct body_data {
		test_data *td;
		struct hasht_st *ht;
	};

	struct body_data bd;
	bd.td = &data;
	bd.ht = hashtable;

	pthread_t pid[10];

	/*
	 * test 2 : multi-thread test
	 */
	int i;
	for (i = 0; i < 3; i++) {
		pthread_create(&pid[i], NULL, adder, &bd);
	}
	for (i = 3; i < 7; i++) {
		pthread_create(&pid[i], NULL, deleter, &bd);
	}
	for (i = 7; i < 10; i++) {
		pthread_create(&pid[i], NULL, finder, &bd);
	}
	for (i = 0; i < 10; i++) {
		pthread_join(pid[i], NULL);
	}

	printf("after thread exit,nodes is %d\n", bd.ht->nr_nodes);
	if (bd.ht->nr_nodes) {
		while (bd.ht->nr_nodes) {
			hasht_delete_item(bd.ht, &hashkey, &data);
		}
		printf("after delete all items, nodes is %d\n", bd.ht->nr_nodes);
	}
	hasht_delete(bd.ht);
	
	printf("%stest2 successful%s\n", COR_BEGIN, COR_END);

	return;
}

int main()
{
	func_test();
	multi_thread_test();
	return 0;
}
#endif
