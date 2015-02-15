#ifndef THR_SCHED_H
#define THR_SCHED_H
/**
    \file thr_sched.h
    Scheduler function
*/

/** cond 0 */
#include "cJSON.h"
/** endcond */

#include "imp.h"

/** Create num worker threads */
int thr_worker_create(int num, cpu_set_t *cpuset);

/** Delete all worker threads */
int thr_worker_destroy(void);

int sched_run(imp_t *imp);
int sched_sleep(imp_t *imp);
int sched_term(imp_t *imp);

cJSON *thr_worker_serialize(void);

#endif

