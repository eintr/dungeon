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
#include "ds_olist.h"
#include "ds_queue.h"
#include "util_mutex.h"
#include "module_handler.h"
#include "ds_stack.h"

/** Dungeon struct. Defines all dungeon core/basic facilities. */
typedef struct dungeon_st {
	int nr_total;		/**< Number of total imps */
	int nr_max;			/**< Max imps limitation */
	ds_stack_t *imp_cache;
	int nr_waitio;		/**< Number of imps waiting for IOEV */

	cpu_set_t process_cpuset;

	queue_t *balance_queue;		/**< A queue for imp migrating between workers. contains imp_t*	*/

	int alert_trap;		/**< To drove those lazy imps sleeping the epoll_fd to run_queue. This is the read-end of a pipe file descriptor */

	int nr_workers;		/**< Number of scheduler threads */
	int nr_busy_workers;  /**< Number of busy scheduler threads */

	int maintainer_quit;/**< A mark notifies maintainer should quit */

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

