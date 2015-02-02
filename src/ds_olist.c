/**	\file ds_olist.c */

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
/** \endcond */

#include "cJSON.h"
#include "util_log.h"
#include "ds_olist.h"

static int olist_random_level(void) {
	int level = 1;

	while ((random() & 0xFFFF) < (OLIST_P * 0xFFFF)) {
		level += 1;
	}

	return (level < OLIST_MAXLEVEL) ? level : OLIST_MAXLEVEL;
}

static olist_node_t *olist_create_node(int level, void *data)
{
	olist_node_t *node = malloc(sizeof(*node) + level * sizeof(struct olist_level));
	node->data = data;
	node->nlevel = level;
	return node;
}

static olist_node_t *olist_insert_node(olist_t *ol, void *data)
{
	olist_node_t *update[OLIST_MAXLEVEL], *x;
	int i, level;

	x = ol->header;
	for (i = ol->level - 1; i >= 0; i--) {
		/* new data insert to front of equal elements */
		while (x->level[i].next && ol->cmp(x->level[i].next->data, data) < 0) {
			x = x->level[i].next;
		}
		update[i] = x;
	}

	level = olist_random_level();
	if (level > ol->level) {
		for (i = ol->level; i < level; i++) {
			update[i] = ol->header;
		}
		ol->level = level;
	}

	x = olist_create_node(level, data);
	for (i = 0; i < level; i++) {
		x->level[i].next = update[i]->level[i].next;
		update[i]->level[i].next = x;
	}

	x->prev = (update[0] == ol->header) ? NULL : update[0];
	if (x->level[0].next) {
		x->level[0].next->prev = x;
	} else {
		ol->tail = x;
	}
	ol->length++;

	return x;
}

static void olist_delete_node(olist_t *ol, olist_node_t *x, olist_node_t **update)
{
	int i;

	for (i = 0; i < ol->level; i++) {
		if (update[i]->level[i].next == x) {
			update[i]->level[i].next = x->level[i].next;
		}
	}

	if (x->level[0].next) {
		x->level[0].next->prev = x->prev;
	} else {
		ol->tail = x->prev;
	}

	while(ol->level > 1 && ol->header->level[ol->level - 1].next == NULL) {
		ol->level--;
	}
	ol->length--;
}

olist_t *olist_new(int max, olist_data_cmp_t *cmp)
{
	int j;
	olist_t *ol;

	ol = malloc(sizeof(*ol));
	if (ol == NULL) {
		return NULL;
	}

	ol->level = 1;
	ol->length = 0;
	ol->volume = max;
	ol->cmp = cmp;
	ol->header = olist_create_node(OLIST_MAXLEVEL, NULL);

	for (j = 0; j < OLIST_MAXLEVEL; j++) {
		ol->header->level[j].next = NULL;
	}
	ol->header->prev = NULL;
	ol->tail = NULL;

	return ol;
}

/*
 * Destroy an llist
 */
int olist_destroy(olist_t *ol)
{
	olist_node_t *node, *next;

	if (ol == NULL) {
		return -1;
	}

	node = ol->header->level[0].next;
	free(ol->header);

	while(node) {
		next = node->level[0].next;
		free(node);
		node = next;
	}

	free(ol);
	return 0;
}

int olist_remove_entry(olist_t *ol, void *data)
{
	int i,ret=-1;
	olist_node_t *update[OLIST_MAXLEVEL], *x;

	if (ol->length <= 0) {
		return ret;
	}

	x = ol->header;
	for (i = ol->level - 1; i >= 0; i--) {
		while (x->level[i].next && ol->cmp(x->level[i].next->data, data) < 0) {
			x = x->level[i].next;
		}
		update[i] = x;
	}

	x = x->level[0].next;
	while(x && ol->cmp(x->data, data) == 0 && x->data != data) {
		for(i = 0; i < x->nlevel; i++) {
			update[i] = x;
		}
		x = x->level[0].next;
	}

	if (x && ol->cmp(x->data, data) == 0 && x->data == data) {
		olist_delete_node(ol, x, update);
		free(x);
		ret = 0;
	}

	return ret;
}

void *olist_search_entry(olist_t *ol, void *data)
{
	int i;
	void *datap=NULL;
	olist_node_t *x;

	if (ol->length <= 0) {
		return datap;
	}

	x = ol->header;
	for (i = ol->level-1; i >= 0; i--) {
		while (x->level[i].next && ol->cmp(x->level[i].next->data, data) < 0) {
			x = x->level[i].next;
		}
	}

	x = x->level[0].next;
	if (x && ol->cmp(x->data, data) == 0) {
		datap = x->data;
	}

	return datap;
}

int olist_add_entry(olist_t *ol, void *data)
{
	int ret;

	if (ol->length+1 > ol->volume) {
		ret = -EAGAIN;
	} else {
		ret = olist_insert_node(ol, data) == NULL ? -1 : 0;
	}

	return ret;
}

/*
 * Get the data ptr of the first node and delete the node.
 */
static void *olist_fetch_head_unlocked(olist_t *ol)
{
	int i;
	void *data;
	olist_node_t *update[OLIST_MAXLEVEL], *node;

	node = ol->header->level[0].next;
	if(node == NULL) {
		return NULL;
	}

	data = node->data;

	for (i = 0 ; i < ol->level; i++) {
		update[i] = ol->header;
	}
	olist_delete_node(ol, node, update);
	free(node);

	return data;
}

void *olist_fetch_head(olist_t *ol)
{
	if (ol->length <= 0) {
		return NULL;
	}

	return olist_fetch_head_unlocked(ol);
}

void *olist_peek_head(olist_t *ol)
{
	void *data;

	data = ol->length <= 0 ? NULL : ol->header->level[0].next->data;

	return data;
}

/*
 * Dump out the info of an olist
 */
cJSON *olist_serialize(olist_t* ol)
{
	cJSON *result;

	result = cJSON_CreateObject();
	cJSON_AddNumberToObject(result, "length", ol->length);
	cJSON_AddNumberToObject(result, "volume", ol->volume);

	return result;
}


#ifdef OL_TEST
void olist_show(olist_t *ol)
{
	int i;
	olist_node_t *node;
	printf("olist info size:%d levels:%d length:%d\n", ol->volume, ol->level, ol->length);
	for (i = ol->level - 1; i >= 0; i--) {
		printf("level %d: ", i);
		node = ol->header;
		while (node->level[i].next) {
			int *data = (int *)node->level[i].next->data;
			printf("%d\t", *data);
			node = node->level[i].next;
		}
		printf("\n");
	}
}

int compare_data(void *a, void *b)
{
	int x = *((int *)a);
	int y = *((int *)b);
	if(x == y) {
		return 0;
	} else if (x > y) {
		return 1;
	} else {
		return -1;
	}
}

int main(int argc, char *argv[]) {
	srand((unsigned)time(0));

	int max,count, i;
	max = atoi(argv[1]) ? atoi(argv[1]) : 32;
	count = atoi(argv[2]) ? atoi(argv[2]) : 20;

	printf("### Function test ###\n");
	printf("Args max:%d length:%d\n", max, count);

	int arr[count];
	printf("=== Init olist_ ===\n");
	olist_t *ol = olist_new(max, compare_data);
	for (i = 0; i < count; i++) {
		arr[i] = i;
		printf("Add[%d]: %s\n", i, olist_add_entry(ol, &arr[i])>=0?"SUCCESS":"FAILURE");
	}

	printf("=== Print olist ===\n");
	olist_show(ol);

	printf("=== Search olist ===\n");
	for (i = 0; i < count; i++) {
		//int value = rand()%(count+10);
		printf("Search[%d]: %s\n", arr[i], olist_search_entry(ol, &arr[i]) != NULL?"SUCCESS":"NOT FOUND");
	}

	int *datap;
	printf("=== Peek head olist ===\n");
	datap = olist_peek_head(ol);
	printf("%d\n", *datap);

	printf("=== Fetch head olist ===\n");
	datap = olist_fetch_head(ol);
	printf("%d\n", *datap);

	printf("=== Fetch head olist no block ===\n");
	datap = olist_fetch_head_nb(ol);
	printf("%d\n", *datap);

	printf("=== Traversal olist ===\n");
	olist_foreach(ol, datap, {
		printf("%d\t", *datap);
	});
	printf("\n");

	printf("=== Delete olist ===\n");
	for (i = 2; i < count; i++) {
		printf("Delete[%d]: %s\n", i, olist_remove_entry(ol, &arr[i])>=0?"SUCCESS":"NOT FOUND");
	}

	olist_show(ol);
	olist_destroy(ol);
	ol = NULL;

	return 0;
}
#endif

