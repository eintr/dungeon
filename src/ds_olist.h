/**
	\file ds_olist.h
	Ordered list, implementated by skip list.
*/

#ifndef OLIST_H
#define OLIST_H

/** \cond 0 */
#include <stdint.h>
#include <pthread.h>
#include "cJSON.h"
/** \endcond */

typedef struct olist_data_st {
	intptr_t data;
	int data_size;
	int key_offset, key_size;
} olist_data_t;

typedef int olist_data_cmp_t(const olist_data_t *a, const olist_data_t *b);

typedef void olist_t;

/**
 * Create a new empty ordered-list.
	\param volume Max number of nodes
	\return Address of the newly created llist structure. NULL when failed.
 */
olist_t *olist_new(int volume, olist_data_cmp_t *cmp);

/**
 * Destroy an ordered-list..
	\return 0 if OK.
 */
int olist_delete(olist_t*);

/**
 * Travel a ordered-list.
	\param ll The linked-list you want to travel
	\param func The call-back function that processes every llist node
	\return 0 if OK
 */
int olist_travel(olist_t*, void (*func)(intptr_t));

/**
 * Add a node to the ordered-list.
 */
void* olist_insert(olist_t*, olist_data_t *data);

/**
 * Get the data ptr of the first node.
 */
int olist_fetch_minimal(olist_t*, void**);

int olist_get_minimal(olist_t*, void**);

/**
 * Dump out the info of an llist
 */
cJSON *olist_info_json(olist_t*);

#endif

