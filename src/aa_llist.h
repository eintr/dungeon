#ifndef LLIST_H
#define LLIST_H

struct llist_node_st {
	struct llist_node_st *prev, *next;
	void *ptr;
};

typedef struct llist_st {
	int nr_nodes;
	int volume;
	struct llist_node_st dumb;
} llist_t;

/*
 * Create a new empty llist
 */
llist_t *llist_new(int);

/*
 * Destroy an llist
 */
int llist_delete(llist_t*);

/*
 * Add a node to the end of the llist
 */
int llist_append(llist_t*, void *datap);
int llist_append_nb(llist_t*, void *datap);

/*
 * Add a node after a node of the llist
 */
int llist_insert_after(llist_t*, void *pos, void *datap);
int llist_insert_after_nb(llist_t*, void *pos, void *datap);

/*
 * Add a node before a node of the llist
 */
int llist_insert_before(llist_t*, void *pos, void *datap);
int llist_insert_before_nb(llist_t*, void *pos, void *datap);

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
 * Dump out the info of an llist
 */
cJSON *llist_info_json(llist_t*);

#endif

