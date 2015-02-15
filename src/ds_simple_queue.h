/** \file	ds_simple_queue.h
	A simple, non-blocked, lockless pointer queue
*/

#ifndef LOCKLESS_QUEUE_H
#define LOCKLESS_QUEUE_H

/** \cond 0 */
#include "cJSON.h"
/** \endcond */

typedef void simqueue_t;

simqueue_t *simqueue_new(int max);
int simqueue_delete(simqueue_t *q);

int simqueue_enqueue(simqueue_t *q, void *data);
int simqueue_dequeue(simqueue_t *q, void **data);

uint32_t simqueue_size(simqueue_t *q);

cJSON *simqueue_info_json(simqueue_t *);

#endif
