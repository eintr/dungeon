/**
    \file thr_scheduler.h
    Worker thread function
*/

/** cond 0 */
#include "cJSON.h"
/** endcond */

#ifndef THR_SCHEDULER_H
#define THR_SCHEDULER_H

/** Create num worker threads
	\param num Number of threads should be created.
	\param cpuset CPU set of threads should bound to.
	\return 0 if OK.
*/
int thr_scheduler_create(int num, cpu_set_t *cpuset);

/** Delete all worker threads
	\return 0 if OK.
*/
int thr_scheduler_destroy(void);

cJSON *thr_scheduler_serialize(void);

#endif

