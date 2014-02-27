#ifndef PROXY_POOL_H
#define PROXY_POOL_H

#include <time.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "ds_llist.h"
#include "cJSON.h"
#include "module_handler.h"
#include "imp_body.h"

typedef struct dungeon_st {
	int nr_total;		// Nr of total contexts
	int nr_max;			// Max context limitation

	llist_t *run_queue;			// of imp_body_t*
	llist_t *terminated_queue;	// of imp_body_t*

	int epoll_fd;
	
	int nr_workers, nr_busy_workers;
	int worker_quit;
	pthread_t *worker;

	int maintainer_quit;
	pthread_t maintainer;

	struct timeval create_time;

	llist_t *rooms;	// of module_handler_t*
} dungeon_t;

/*
 * Create a pool
 */
dungeon_t *dungeon_new(int nr_workers, int nr_max);

/*
 * Module process
 */
//int dungeon_add_room(dungeon_t *, const char *fname, cJSON *conf);
//int dungeon_demolish_allroom(dungeon_t *);
//int dungeon_demolish_room(dungeon_t *, const char *fname);	// TODO

/*
 * Context process
 */
void imp_set_run(dungeon_t *, imp_body_t *);
void imp_set_iowait(dungeon_t *, int fd, imp_body_t *);
void imp_set_term(dungeon_t *, imp_body_t *);

/*
 * Delete a proxy pool
 */
int dungeon_delete(dungeon_t*);

/*
 * Dump the whole pool info in JSON
 */
cJSON *dungeon_serialize(dungeon_t*);

#endif

