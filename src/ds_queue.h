/** \file	ds_queue.h
	A simple, non-blocked, lock-free pointer queue
*/

#ifndef _AA_QUEUE_H
#define _AA_QUEUE_H

/** \cond 0 */
#include <stdint.h>
/** \endcond */

typedef void (* travel_func) (void *);

typedef void queue_t;

queue_t *queue_new(uint32_t max);

int queue_enqueue_nb(queue_t *q, void *data);
int queue_enqueue(queue_t *q, void *data);

int queue_dequeue_nb(queue_t *q, void **data);
int queue_dequeue(queue_t *q, void **data);

uint32_t queue_size(queue_t *q);

void queue_travel(queue_t *q, travel_func tf);

int queue_delete(queue_t *q);

cJSON *queue_info_json(queue_t *);

#endif
