/**	\file ds_olist.c */

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "ds_olist.h"

#include "util_log.h"

/** Ordered-list node structure */
struct olist_data_entry_st {
	struct olist_data_entry_st *next;
	olist_data_t *data;	/**< Points to user-allocated memory */
};

/** Ordered-list structure */
struct olist_st {
	int nr_nodes;				/**< Number of nodes contained */
	int volume;					/**< Max number of nodes */
	struct olist_data_entry_st *index0;				/**< Root index */
	struct olist_data_entry_st *index1;				/**< Sub index */
	struct olist_data_entry_st	*head;	/**< Data entries */
	olist_data_cmp_t	*cmp;	/**< Compare function */
	pthread_mutex_t lock;		/**< Mutual lock */
	pthread_cond_t cond;		/**< Condition variable */
};

/*
 * return value:
 * 	0 success
 * 	<0 -errno
 */

/**
 * Create a new empty ordered-list
 */
olist_t *olist_new(int max, olist_data_cmp_t *cmp)
{
	struct olist_st *l;

	l = malloc(sizeof(*l));
	if (l == NULL) {
		return NULL;
	}

	l->nr_nodes = 0;
	l->volume = max;
	l->index0 = NULL;
	l->index1 = NULL;
	l->head = NULL;
	l->cmp = cmp;

	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&l->lock, &ma) != 0) {
		free(l);
		return NULL;
	}
	if (pthread_cond_init(&l->cond, NULL) != 0) {
		pthread_mutex_destroy(&l->lock);
		free(l);
		return NULL;
	}

	pthread_mutexattr_destroy(&ma);
	return l;
}

static struct olist_data_entry_st *olist_new_data_entry(void *p)
{
	struct olist_data_entry_st *node;
	node = malloc(sizeof(*node));
	if (node == NULL) {
		return NULL;
	}
	node->data = p;
	node->next = NULL;
	return node;
}


/*
 * Destroy an olist
 */
int olist_delete(olist_t *ptr)
{
	struct olist_st *l=ptr;
	//void *buf;

	if (l == NULL) {
		return 0;
	}

	//while (l->nr_nodes > 0) {
	//	llist_fetch_head_nb(l, &buf);
	//}

	pthread_mutex_destroy(&l->lock);
	pthread_cond_destroy(&l->cond);

	free(l);
	return 0;
}

static void olist_insert_data_entry_unlocked(struct olist_st *l, struct olist_data_entry_st *ptr)
{
	struct olist_data_entry_st *cur;

	if (l->head==NULL) {
		l->head = ptr;
		return;
	}

	if (l->cmp(l->head->data, ptr->data)>=0) {
		ptr->next = l->head;
		l->head = ptr;
		return;
	}

	for (cur=l->head; cur!=NULL; cur=cur->next) {
		if (cur->next == NULL) {
				ptr->next = cur->next;
				cur->next = ptr;
				break;
		} else {
			if (l->cmp(cur->data, ptr->data)<0 && l->cmp((cur->next)->data, ptr->data)>=0) {
				ptr->next = cur->next;
				cur->next = ptr;
				break;
			}
		}
	}
	return;
}

/*
 * Add a node to the olist
 */
static struct olist_data_entry_st *olist_insert_unlocked(struct olist_st *l, olist_data_t *p)
{
	struct olist_data_entry_st *node;

	if (l->nr_nodes < l->volume) {
		node = olist_new_data_entry(p);
		if (node == NULL) {
			return NULL;
		}

		olist_insert_data_entry_unlocked(l, node);

		return node;
	}
	return NULL;
}

void* olist_insert(olist_t *self, olist_data_t *data)
{
	struct olist_st *l=self;
	void* ret;

	pthread_mutex_lock(&l->lock);
	while (l->nr_nodes >= l->volume) {
		pthread_cond_wait(&l->cond, &l->lock);
	}

	ret = olist_insert_unlocked(l, data);
	l->nr_nodes++;

	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->lock);
	return ret;
}

/*
 * Get the data ptr of the first node and delete the node.
 */
static int llist_fetch_minimal_unlocked(struct olist_st *l, void **data)
{
	struct olist_data_entry_st *tmp;
	
	if (l->nr_nodes <= 0) {
		return -1;
	}

	tmp = l->head->next;
	*data = l->head->data;
	free(l->head);
	l->head = tmp;

	return 0;
}

int olist_fetch_minimal(olist_t *ptr, void **data)
{
	struct olist_st *l=ptr;
	int ret;

	pthread_mutex_lock(&l->lock);
	while (l->nr_nodes <= 0) {
		pthread_cond_wait(&l->cond, &l->lock);
	}

	ret = llist_fetch_minimal_unlocked(l, data);
	l->nr_nodes--;

	pthread_cond_signal(&l->cond);
	pthread_mutex_unlock(&l->lock);
	return ret;
}

int olist_travel(olist_t *ptr, void (*func)(intptr_t))
{
	struct olist_st *l=ptr;
	struct olist_data_entry_st *cur;

	if (func == NULL)
		return 0;

	pthread_mutex_lock(&l->lock);
	for (cur = l->head; cur != NULL; cur = cur->next) {
		func(cur->data->data);
	}
	pthread_mutex_unlock(&l->lock);
	return 0;
}


/*
 * Dump out the info of an llist
 */
cJSON *olist_info_json(olist_t* ptr)
{
	struct olist_st *l=ptr;
	cJSON *result;

	result = cJSON_CreateObject();
	cJSON_AddNumberToObject(result, "nr_nodes", l->nr_nodes);
	cJSON_AddNumberToObject(result, "volume", l->volume);

	return result;
}

