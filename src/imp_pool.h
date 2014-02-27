#ifndef PROXY_POOL_H
#define PROXY_POOL_H

#include <time.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "ds_llist.h"
#include "cJSON.h"
#include "module_handler.h"
#include "gen_context.h"

typedef struct imp_pool_st {
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
} imp_pool_t;

/*
 * Create a pool
 */
imp_pool_t *imp_pool_new(int nr_workers, int nr_max);

/*
 * Module process
 */
int imp_pool_load_module(imp_pool_t *, const char *fname, cJSON *conf);
int imp_pool_unload_allmodule(imp_pool_t *);
int imp_pool_unload_module(imp_pool_t *, const char *fname);	// TODO

/*
 * Context process
 */
void imp_set_run(imp_pool_t *, generic_context_t *);
void imp_set_iowait(imp_pool_t *, int fd, generic_context_t *);
void imp_set_term(imp_pool_t *, generic_context_t *);

/*
 * Delete a proxy pool
 */
int imp_pool_delete(imp_pool_t*);

/*
 * Dump the whole pool info in JSON
 */
cJSON *imp_pool_serialize(imp_pool_t*);

#endif

