#ifndef LLIST_H
#define LLIST_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <pthread.h>

#include "cJSON.h"

typedef struct llist_node_st {
	struct llist_node_st *prev, *next;
	void *ptr;
} llist_node_t;

typedef struct llist_st {
	int nr_nodes;
	int volume;
	llist_node_t *dumb;
	pthread_mutex_t lock;
	pthread_cond_t cond;
} llist_t;

/*
 * Create a new empty llist
 */
llist_t *llist_new(int);

/*
 * Destroy a llist
 */
int llist_delete(llist_t*);

/*
 * Travel a llist
 */
int llist_travel(llist_t *ll, void (*func)(void *data));
int llist_travel_nb(llist_t *ll, void (*func)(void *data));

/*
 * Add a node to the end of the llist
 */
int llist_append(llist_t*, void *data);
int llist_append_nb(llist_t*, void *data);

/*
 * Add a node after a node of the llist
 */
//int llist_insert_after(llist_t*, void *pos, void *datap);
//int llist_insert_after_nb(llist_t*, void *pos, void *datap);

/*
 * Add a node before a node of the llist
 */
//int llist_insert_before(llist_t*, void *pos, void *datap);
//int llist_insert_before_nb(llist_t*, void *pos, void *datap);

/*
 * Get the data ptr of the first node.
 */
int llist_get_head(llist_t*, void**);
int llist_get_head_nb(llist_t*, void**);

/*
 * Get the data ptr of the first node and delete the node.
 */
int llist_fetch_head(llist_t*, void**);
int llist_fetch_head_nb(llist_t*, void**);

/*
 * Get the next node of ptr
 */
//void * llist_get_next_unlocked(llist_t *ll, void *ptr);
void * llist_get_next_nb(llist_t *ll, void *ptr);

/*
 * Get the first node
 */
//int llist_get_head_node_unlocked(llist_t *ll, void **node);
int llist_get_head_node_nb(llist_t *ll, void **node);
/*
 * Dump out the info of an llist
 */
cJSON * llist_info_json(llist_t *ll);

#endif

