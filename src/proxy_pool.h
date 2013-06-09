#ifndef PROXY_POOL_H
#define PROXY_POOL_H

#include <pthread.h>

#include "aa_llist.h"
#include "cJSON.h"

typedef struct proxy_pool_st {
	int nr_idle, nr_busy, nr_accepters, nr_total, nr_max;
	llist_t *run_queue;
	int epoll_pool;
	llist_t *terminated_queue;
	int original_listen_sd;

	int nr_workers;
	int worker_quit;
	pthread_t *worker;
	int maintainer_quit;
	pthread_t maintainer;
} proxy_pool_t;

/*
 * Create a proxy pool
 */
proxy_pool_t *proxy_pool_new(int nr_workers, int nr_accepters, int nr_max, int nr_total, int listen_sd);

int proxy_pool_delete(proxy_pool_t*);

/*
 * Dump the whole pool info in JSON
 */
cJSON *proxy_pool_info(proxy_pool_t*);

#endif

