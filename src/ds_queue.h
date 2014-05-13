/** \file	ds_queue.h
	A simple, non-blocked, lock-free pointer queue
*/

#ifndef _AA_QUEUE_H
#define _AA_QUEUE_H

/** \cond 0 */
#include <stdint.h>
/** \endcond */

#define CAS(ptr, oldv, newv) __sync_bool_compare_and_swap(ptr, oldv, newv)

typedef void (* travel_func) (void *);

typedef struct {
	uint32_t max;
	uint32_t max_rindex;
	uint32_t rindex;
	uint32_t windex;
	void *bucket[0];
} queue_t;

queue_t *queue_init(uint32_t max);

int queue_enqueue_nb(queue_t *q, void *data);

int queue_dequeue_nb(queue_t *q, void **data);

uint32_t queue_size(queue_t *q);

void queue_travel(queue_t *q, travel_func tf);

int queue_destroy(queue_t *q);

#endif
