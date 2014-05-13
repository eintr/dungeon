/**
	\file dungeon.h
	Dungeon defination. Heart of this software.
*/

#ifndef DUNGEON_H
#define DUNGEON_H

/** \cond 0 */
#include <time.h>
#include <pthread.h>
#include <sys/epoll.h>
#include "cJSON.h"
/** \endcond */

#include "ds_llist.h"
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

/** The global dungeon instanse */
extern dungeon_t *dungeon_heart;

/**
 * Create a dungeon
	\param nr_workers Worker thread number. Should match the cpu number.
	\param nr_max Max limitation of imps in the dungeon.
	\return 0 if OK.
 */
int dungeon_init(int nr_workers, int nr_max);

/**
 * Delete a dungeon. As well as all the contained resources.
 */
int dungeon_delete(void);

/**
 * Dump the dungeon info in JSON
 */
cJSON *dungeon_serialize(void);

#endif

