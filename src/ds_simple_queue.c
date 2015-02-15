/** \file ds_queue.c
*/

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include "cJSON.h"
/** \endcond */

#include "ds_simple_queue.h"

struct simqueue_st {
	uint32_t max, nr;
	uint32_t rindex;
	uint32_t windex;
	void *bucket[0];
};

static uint32_t next(uint32_t n, uint32_t max)
{
	++n;
	if (n>=max) {
		return 0;
	}
	return n;
}

simqueue_t *simqueue_new(int max)
{
	struct simqueue_st *q;
	q = malloc(sizeof(*q) + max*sizeof(void*));
	if (q == NULL) {
		return NULL;
	}
	q->max = max;
	q->nr = 0;
	q->rindex = q->windex = 0;
	return q;
}

int simqueue_enqueue(simqueue_t *ptr, void *data)
{
	struct simqueue_st *q=ptr;
	if (q->nr>=q->max) {
		return -EAGAIN;
	}
	q->bucket[q->windex] = data;
	q->windex = next(q->windex, q->max);
	q->nr++;

	return 0;
}
	
int simqueue_dequeue(simqueue_t *ptr, void **data)
{
	struct simqueue_st *q=ptr;

	if (q->nr==0) {
		return -EAGAIN;
	}
	*data = q->bucket[q->rindex];
	q->rindex = next(q->rindex, q->max);
	q->nr--;

	return 0;
}

uint32_t simqueue_size(simqueue_t *ptr)
{
	struct simqueue_st *q=ptr;
	return q->nr;
}

int simqueue_delete(simqueue_t *ptr)
{
	struct simqueue_st *q=ptr;
	if (q == NULL) 
		return 0;
	free(q);
	return 0;
}

cJSON *simueue_info_json(simqueue_t *ptr)
{
	struct simqueue_st *q=ptr;
    cJSON *result;

    result = cJSON_CreateObject();
    cJSON_AddNumberToObject(result, "volume", q->max);
    cJSON_AddNumberToObject(result, "nr_data", simqueue_size(q));

    return result;
}

