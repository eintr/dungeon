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

llist_t *llist_new(int);

int llist_delete(llist_t*);

int llist_append(llist_t*, void *datap);
int llist_append_nb(llist_t*, void *datap);

int llist_insert_after(llist_t*, void *pos, void *datap);
int llist_insert_after_nb(llist_t*, void *pos, void *datap);

int llist_get_head(llist_t*, void**);
int llist_get_head_nb(llist_t*, void**);

int llist_fetch_head(llist_t*, void**);
int llist_fetch_head_nb(llist_t*, void**);

cJSON *llist_info_json(llist_t*);

#endif

