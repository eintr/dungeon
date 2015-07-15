#ifndef DS_OLIST_H
#define DS_OLIST_H

/** \cond 0 */
#include <stdint.h>
#include <pthread.h>
#include "cJSON.h"
/** \endcond  */

#define OLIST_MAXLEVEL 8
#define OLIST_P 0.25

typedef int olist_data_cmp_t(void *, void *);

typedef struct olist_node_st {
	void *data;
	int nlevel;
	struct olist_node_st *prev;
	struct olist_level {
		struct olist_node_st *next;
	} level[];
} olist_node_t;

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

#define	olist_foreach(ol, datap, statement) do {olist_node_t *_x_; olist_lock(ol); _x_ = ol->header; while (_x_->level[0].next) { datap = _x_->level[0].next->data; {statement}; _x_ = _x_->level[0].next;} olist_unlock(ol);} while(0)

#if 0
#define	olist_iterate_begin(datap, ol) {olist_node_t *_x_; olist_lock(ol); _x_ = ol->header; while (_x_->level[0].next) { datap = _x_->level[0].next->data;
#define	olist_iterate_break	goto _olist_iter_exit_;
#define olist_iterate_end(ol)	_x_ = _x_->level[0].next;} _olist_iter_exit_: olist_unlock(ol);}
#endif

cJSON *olist_serialize(olist_t* ol);

#endif
