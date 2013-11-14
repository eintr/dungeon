#ifndef PROXY_POOL_H
#define PROXY_POOL_H

#include <time.h>
#include <pthread.h>

#include "aa_llist.h"
#include "cJSON.h"
#include "aa_module_handler.h"
#include "aa_module_interface.h"

typedef struct proxy_pool_st {
	int nr_total;		// Nr of total contexts
	int nr_max;			// Max context limitation

	llist_t *run_queue;			// of generic_context_t*
	llist_t *terminated_queue;	// of generic_context_t*

	int epoll_fd;
	
	int nr_workers, nr_busy_workers;
	int worker_quit;
	pthread_t *worker;

	int maintainer_quit;
	pthread_t maintainer;

	struct timeval create_time;

	llist_t *modules;	// of module_handler_t*
} proxy_pool_t;

/*
 * Create a proxy pool
 */
proxy_pool_t *proxy_pool_new(int nr_workers, int nr_max);

/*
 * Module process
 */
int proxy_pool_load_module(proxy_pool_t *, const char *fname);
int proxy_pool_unload_allmodule(proxy_pool_t *);
int proxy_pool_unload_module(proxy_pool_t *, const char *fname);	// TODO

/*
 * Context process
 */
void proxt_pool_set_run(proxy_pool_t *, void *);
void proxt_pool_set_iowait(proxy_pool_t *, int fd, void*);
void proxt_pool_set_term(proxy_pool_t *, void *);

/*
 * Delete a proxy pool
 */
int proxy_pool_delete(proxy_pool_t*);

/*
 * Dump the whole pool info in JSON
 */
cJSON *proxy_pool_serialize(proxy_pool_t*);

#endif

