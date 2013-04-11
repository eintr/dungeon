#ifndef PROXY_POOL_H
#define PROXY_POOL_H

#include "proxy_context.h"

typedef struct proxy_pool_st {
	int nr_minidle, nr_maxidle, nr_total;
	int nr_idle, nr_busy;
	llist_t *run_queue;
//	hasht_t	*iowait_queue_ht;
	llist_t *terminated_queue;
	int original_listen_sd;

	int nr_workers;
	int worker_quit;
	pthread_t *worker;
	int maintainer_quit;
	pthread_t maintainer;
} proxy_pool_t;

proxy_pool_t *proxy_pool_new(int nr_workers, int nr_minidle, int nr_maxidle, int nr_total, int listensd);

int proxy_pool_delete(proxy_pool_t*);

cJSON *proxy_pool_info(proxy_pool_t*);

#endif

