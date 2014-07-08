/**
    \file thr_worker.h
    Worker thread function
*/

/** cond 0 */
#include "cJSON.h"
/** endcond */

#ifndef THR_WORKER_H
#define THR_WORKER_H

//extern __thread imp_t *current;

/** Create num worker threads */
int thr_worker_create(int num);

/** Delete all worker threads */
int thr_worker_destroy(void);

cJSON *thr_worker_serialize(void);

#endif

