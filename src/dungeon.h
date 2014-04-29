/**
	\file dungeon.h
	Dungeon defination. Heart of this software.
*/

#ifndef DUNGEON_H
#define DUNGEON_H

#include <time.h>
#include <pthread.h>
#include <sys/epoll.h>

#include "ds_llist.h"
#include "cJSON.h"
#include "module_handler.h"

/** Dungeon struct. Defines all dungeon core/basic facilities. */
typedef struct dungeon_st {
	int nr_total;		/**< Number of total imps */
	int nr_max;			/**< Max imps limitation */

	llist_t *run_queue;			/**< Run queue. contains imp_t*	*/
	llist_t *terminated_queue;	/**< Terminated queue. contains imp_t* */

	int epoll_fd;		/**< Event engine. This is an epoll file descriptor */
	
	int nr_workers;		/**< Number of worker threads */
	int nr_busy_workers;/**< Number of busy worker threads */
	int worker_quit;	/**< A mark notifies workers should quit */
	pthread_t *worker;	/**< IDs of worker threads */

	int maintainer_quit;/**< A mark notifies maintainer should quit */
	pthread_t maintainer;/**< ID of maintainer thread */

	struct timeval create_time;/**< Time of dungeon creation */

 	llist_t *rooms;		/**< All loaded rooms. contains module_handler_t* */
} dungeon_t;

/**
 * Create a dungeon
	\param nr_workers Worker thread number. Should match the cpu number.
	\param nr_max Max limitation of imps in the dungeon.
	\return The address of the newly created dungeon structure. NULL when failed.
 */
dungeon_t *dungeon_new(int nr_workers, int nr_max);

/*
 * Module process
 */
//int dungeon_add_room(dungeon_t *, const char *fname, cJSON *conf);
//int dungeon_demolish_room(dungeon_t *, const char *fname);	// TODO

/**
 * Delete a dungeon. As well as all the contained resources.
 */
int dungeon_delete(dungeon_t*);

/**
 * Dump the dungeon info in JSON
 */
cJSON *dungeon_serialize(dungeon_t*);

#endif

