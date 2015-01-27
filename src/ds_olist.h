#ifndef DS_OLIST_H
#define DS_OLIST_H

/** \file ds_olist.h
        A list which can maintain element order.
*/

/** \cond 0 */
#include <stdint.h>
#include <pthread.h>
#include "cJSON.h"
/** \cond 0 */

/**
    \def OLIST_MAXLEVEL
        How many level indexed can make at most.
*/
#define OLIST_MAXLEVEL 8

/**
    \def OLIST_P
        The possibility of a node should create index node.
*/
#define OLIST_P 0.25F

/** The function of element comparing */
typedef int olist_data_cmp_t(void *, void *);

/** olist_node defination */
typedef struct olist_node_st {
	void *data;
	int nlevel;
	struct olist_node_st *prev;
	struct olist_level {
		struct olist_node_st *next;
	} level[];
} olist_node_t;

/** olist defination */
typedef struct olist_st {
	struct olist_node_st *header;
	struct olist_node_st *tail;
	unsigned long length;
	int level;
	int volume;

	olist_data_cmp_t *cmp;
	pthread_mutex_t lock;
	pthread_cond_t cond;
} olist_t;

olist_t *olist_new(int volume, olist_data_cmp_t *cmp);
int olist_destroy(olist_t *ol);

int olist_remove_entry(olist_t *ol, void *data);
void *olist_search_entry(olist_t *ol, void *data);

int olist_add_entry(olist_t *ol, void *data);
int olist_add_entry_nb(olist_t *ol, void *data);

void olist_lock(olist_t *ol);
void olist_unlock(olist_t *ol);

void *olist_fetch_head(olist_t *ol);
void *olist_fetch_head_nb(olist_t *ol);
void *olist_peek_head(olist_t *ol);

/**
    \def olist_foreach
        A rough macro to interate an olist.
	\param ol The olist.
	\param datap The address of a data pointer value.
	\param statement The C statement of interating.
*/
#define	olist_foreach(ol, datap, statement) do {olist_node_t *_x_; olist_lock(ol); _x_ = ol->header; while (_x_->level[0].next) { datap = _x_->level[0].next->data; {statement}; _x_ = _x_->level[0].next;} olist_unlock(ol);} while(0)

cJSON *olist_serialize(olist_t* ol);

#endif
