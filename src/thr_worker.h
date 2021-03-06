/**
    \file thr_worker.h
    Worker thread function
*/

/** cond 0 */
#include "cJSON.h"
/** endcond */

#ifndef THR_WORKER_H
#define THR_WORKER_H

/** Create num worker threads */
int thr_worker_create(int num, cpu_set_t *cpuset);

/** Delete all worker threads */
int thr_worker_destroy(void);

cJSON *thr_worker_serialize(void);

#endif

