#include "aa_llist.h"

/*
 * Create a new empty llist
 */
llist_t *llist_new(int max)
{
	llist_t *ll;
	llist_node *ln;

	ll = (llist_t *) malloc(sizeof(llist_t));
	if (ll == NULL) {
		return NULL;
	}
	ln = (llist_node *) malloc(sizeof(llist_node));
	if (ln == NULL) {
		free(ll);
		return NULL;
	}
	
	ll->dumb = ln;
	ll->nr_nodes = 0;
	ll->volume = max;
	ll->dumb->prev = ll->dumb->next = ll->dumb;
	ll->dumb->ptr = NULL;

	pthread_mutexattr_t ma;
	pthread_mutexattr_init(&ma);
	pthread_mutexattr_settype(&ma, PTHREAD_MUTEX_RECURSIVE);

	if (pthread_mutex_init(&ll->lock, &ma) != 0) {
		free(ll);
		return NULL;
	}
	if (pthread_cond_init(&ll->cond, NULL) != 0) {
		pthread_mutex_destroy(&ll->lock);
		free(ll);
		return NULL;
	}

	pthread_mutexattr_destroy(&ma);
	return ll;
}

llist_node * llist_new_node()
{
	llist_node *lnode;
	lnode = (llist_node *) malloc(sizeof(llist_node));
	if (lnode == NULL) {
		return NULL;
	}
	lnode->ptr = NULL;
	lnode->prev = lnode->next = NULL;
	return lnode;
}


/*
 * Destroy an llist
 */
int llist_delete(llist_t *ll)
{
	void *buf;

	while (llist_fetch_head_unlocked(ll, &buf) != -1);

	pthread_mutex_destroy(&ll->lock);
	pthread_cond_destroy(&ll->cond);

	free(ll->dumb);
	free(ll);
	return 0;
}

/*
 * Add a node to the end of the llist
 */
int llist_append_unlocked(llist_t *ll, void *data)
{
	llist_node *lnode;
	if (ll->nr_nodes < ll->volume) {
		lnode = llist_new_node();
		if (lnode == NULL) {
			return -1;
		}
		lnode->ptr = data;

		lnode->prev = ll->dumb->prev;
		lnode->next = ll->dumb;
		ll->dumb->prev->next = lnode;
		ll->dumb->prev = lnode;
		ll->nr_nodes++;

		return 0;
	}
	return -1;
}

int llist_append(llist_t *ll, void *data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes >= ll->volume) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_append_unlocked(ll, data);

	pthread_cond_signal(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_append_nb(llist_t *ll, void *data)
{
	int ret;

	if (pthread_mutex_trylock(&ll->lock) != 0)  {
		return -2;
	}

	ret = llist_append_unlocked(ll, data);
	
	pthread_cond_signal(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

/*
 * Add a node after a node of the llist
 */
//int llist_insert_after(llist_t*, void *pos, void *data);
//int llist_insert_after_nb(llist_t*, void *pos, void *data);

/*
 * Add a node before a node of the llist
 */
//int llist_insert_before(llist_t*, void *pos, void *data);
//int llist_insert_before_nb(llist_t*, void *pos, void *data);

/*
 * Get the data ptr of the first node.
 */
int llist_get_head_unlocked(llist_t *ll, void **data) 
{
	llist_node *ln;
	if (ll == NULL) {
		return -1;
	}
	if (ll->nr_nodes <= 0) {
		return -1;
	}
	ln = ll->dumb->next;
	if (ln != ll->dumb) {
		*data = &ln->ptr;
		return 0;
	}
	return -1;
}

int llist_get_head(llist_t *ll, void **data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes <= 0) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_get_head_unlocked(ll, data);

	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_get_head_nb(llist_t *ll, void **data)
{
	int ret;

	if (pthread_mutex_trylock(&ll->lock) != 0) {
		return -2;
	}

	ret = llist_get_head_unlocked(ll, data);

	pthread_mutex_unlock(&ll->lock);
	return ret;
}

/*
 * Get the data ptr of the first node and delete the node.
 */
int llist_fetch_head_unlocked(llist_t *ll, void **data)
{
	llist_node *ln;
	int ret;

	if (ll == NULL) {
		return -1;
	}
	if (ll->nr_nodes <= 0) {
		return -1;
	}
	ln = ll->dumb->next;
	if (ln != ll->dumb) {
		*data = &ln->ptr;
		ll->dumb->next = ln->next;
		ln->next->prev = ll->dumb;
		free(ln);
		ll->nr_nodes--;
		return 0;
	}
	return -1;
}

int llist_fetch_head(llist_t *ll, void **data)
{
	int ret;

	pthread_mutex_lock(&ll->lock);
	while (ll->nr_nodes <= 0) {
		pthread_cond_wait(&ll->cond, &ll->lock);
	}

	ret = llist_fetch_head_unlocked(ll, data);

	pthread_cond_signal(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_fetch_head_nb(llist_t *ll, void **data)
{
	int ret;

	if (pthread_mutex_trylock(&ll->lock) != 0) {
		return -2;
	}

	ret = llist_fetch_head_unlocked(ll, data);

	pthread_cond_signal(&ll->cond);
	pthread_mutex_unlock(&ll->lock);
	return ret;
}

int llist_travel(llist_t *ll, void (*func)(void *data))
{
	llist_node *ln;
	if (func == NULL)
		return -1;

	for (ln = ll->dumb->next; ln != ll->dumb; ln = ln->next) {
		if (ln->ptr) {
			func(ln->ptr);
		}
	}
	return 0;
}

/*
 * Dump out the info of an llist
 */
//cJSON *llist_info_json(llist_t*);

#ifdef AA_LIST_TEST

void show(void *d)
{
	printf("%c", *(char *)d);
}


void * producer(void *args)
{
	int l=0;
	int num = 0;
	llist_t *ll  = (llist_t *)args;
	char *s = "c";
	while(l++<10000) {
		if (llist_append_nb(ll, &s[0]) == 0)
			num++;
	}
	printf("append success %d\n", num);
}
void * consumer(void *args)
{
	int l=0;
	llist_t *ll = (llist_t *)args;
	void *buf;
	int num =0;
	while(l++<10000) {
		if(llist_fetch_head_nb(ll, &buf) == 0)
			num++;
	}
	printf("fetch success %d\n", num);
}

void func_test()
{
	char *s = "this is a test";
	llist_t *ll;
	ll = llist_new(10);	
	int i;
	for (i = 0; i < 10; i++) {
		llist_append(ll, &s[i]);
	}
	printf("******* test 1 ******\n");
	llist_travel(ll, &show);
	printf("\n");
	void *b;
	llist_fetch_head(ll, &b);
	llist_travel(ll, &show);
	printf("\n");
	llist_fetch_head(ll, &b);
	llist_travel(ll, &show);
	printf("\n");
	llist_delete(ll);
	return;
}

void multi_thread_test()
{
	llist_t *ll;
	ll = llist_new(1000);	
	pthread_t pid[10];

	printf("******* test 2 ******\n");
	int i;
	for (i=0; i < 6; i++) {
		pthread_create(&pid[i], NULL, producer, ll);
	}
	for (i=6; i<10; i++) {
		pthread_create(&pid[i], NULL, consumer, ll);
	}
	for (i=0; i<10; i++) {
		pthread_join(pid[i], NULL);
	}
	printf("nodes is %d\n", ll->nr_nodes);
	llist_delete(ll);
}

int main()
{
	func_test();
	multi_thread_test();
	return 0;
}
#endif

