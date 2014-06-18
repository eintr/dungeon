/** \file ds_queue.c
*/

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "cJSON.h"
/** \endcond */

#include "ds_queue.h"

#define CAS(ptr, oldv, newv) __sync_bool_compare_and_swap(ptr, oldv, newv)

struct queue_st {
	uint32_t max;
	uint32_t max_rindex;
	uint32_t rindex;
	uint32_t windex;
	void *bucket[0];
};

queue_t *queue_new(uint32_t max)
{
	struct queue_st *q;
	q = malloc(sizeof(*q) + max*sizeof(void*));
	if (q == NULL) {
		return NULL;
	}
	q->max = max;
	q->rindex = q->windex = q->max_rindex = 0;
	return q;
}

int queue_enqueue_nb(queue_t *ptr, void *data)
{
	struct queue_st *q=ptr;
	uint32_t rindex;
	uint32_t windex;

	do {
		windex = q->windex;
		rindex  = q->rindex;
		if (((windex + 1) % q->max) == (rindex % q->max)) {
			return -1; // the queue is full
		}
	} while (!CAS(&q->windex, windex, (windex + 1) % q->max));

	q->bucket[windex] = data;

	while (!CAS(&q->max_rindex, windex, (windex + 1) % q->max)) {
		sched_yield();
	}
	return 0;
}
	
int queue_dequeue_nb(queue_t *ptr, void **p)
{
	struct queue_st *q=ptr;
	uint32_t max_rindex;
	uint32_t rindex;

	do {
		rindex = q->rindex;
		max_rindex = q->max_rindex;
		if ((rindex % q->max) == (max_rindex % q->max)) {
			return -1; // queue is empty
		}
		*p = q->bucket[rindex];
	} while(!CAS(&q->rindex, rindex, (rindex + 1) % q->max)); 

	return 0;
}

uint32_t queue_size(queue_t *ptr)
{
	struct queue_st *q=ptr;
	if (q->windex >= q->rindex) {
		return q->windex - q->rindex;
	} else {
		return q->max + q->windex - q->rindex;
	}
}

void queue_travel(queue_t *ptr, travel_func tf)
{
	struct queue_st *q=ptr;
	uint32_t idx;
	idx = q->rindex;
	while (idx != q->max_rindex) {
		tf(q->bucket[idx]);
		idx = (idx + 1) % q->max;
	}
}

int queue_delete(queue_t *ptr)
{
	struct queue_st *q=ptr;
	if (q == NULL) 
		return -1;
	free(q);
	return 0;
}

cJSON *queue_info_json(queue_t *ptr)
{
	struct queue_st *q=ptr;
    cJSON *result;

    result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "volume", q->max);
    cJSON_AddNumberToObject(result, "nr_data", queue_size(q));

    return result;
}

