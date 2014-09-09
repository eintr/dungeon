/** \file ds_queue.c
*/

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <pthread.h>
#include "cJSON.h"
/** \endcond */

#include "ds_queue.h"

struct queue_st {
	uint32_t max, nr;
	pthread_mutex_t mut;
	pthread_cond_t cond_eq;
	pthread_cond_t cond_dq;
	uint32_t en_count, de_count;
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

queue_t *queue_new(uint32_t max)
{
	struct queue_st *q;
	q = malloc(sizeof(*q) + max*sizeof(void*));
	if (q == NULL) {
		return NULL;
	}
	q->max = max;
	pthread_mutex_init(&q->mut, NULL);
	pthread_cond_init(&q->cond_eq, NULL);
	pthread_cond_init(&q->cond_dq, NULL);
	q->nr = 0;
	q->en_count = 0;
	q->de_count = 0;
	q->rindex = q->windex = 0;
	return q;
}

static void cleanup_unlock(pthread_mutex_t *mut)
{
	pthread_mutex_unlock(mut);
}

int queue_enqueue(queue_t *ptr, void *data)
{
	struct queue_st *q=ptr;

	pthread_mutex_lock(&q->mut);
	pthread_cleanup_push(cleanup_unlock, &q->mut);
	while (q->nr>=q->max) {
		pthread_cond_wait(&q->cond_eq, &q->mut);
	}
	q->bucket[q->windex] = data;
	q->windex = next(q->windex, q->max);
	q->nr++;
	q->en_count++;
	pthread_cond_signal(&q->cond_dq);
	pthread_cleanup_pop(1);

	return 0;
}
	
int queue_dequeue(queue_t *ptr, void **data)
{
	struct queue_st *q=ptr;

	pthread_mutex_lock(&q->mut);
	pthread_cleanup_push(cleanup_unlock, &q->mut);
	while (q->nr<=0) {
		pthread_cond_wait(&q->cond_dq, &q->mut);
	}
	*data = q->bucket[q->rindex];
	q->rindex = next(q->rindex, q->max);
	q->nr--;
	q->de_count++;
	pthread_cond_signal(&q->cond_eq);
	pthread_cleanup_pop(1);

	return 0;
}

int queue_enqueue_nb(queue_t *ptr, void *data)
{
	struct queue_st *q=ptr;

	pthread_mutex_lock(&q->mut);
	if (q->nr>=q->max) {
		pthread_mutex_unlock(&q->mut);
		return -1;
	}
	q->bucket[q->windex] = data;
	q->windex = next(q->windex, q->max);
	q->nr++;
	q->en_count++;
	pthread_cond_signal(&q->cond_dq);
	pthread_mutex_unlock(&q->mut);

	return 0;
}
	
int queue_dequeue_nb(queue_t *ptr, void **data)
{
	struct queue_st *q=ptr;

	pthread_mutex_lock(&q->mut);
	if (q->nr<=0) {
		pthread_mutex_unlock(&q->mut);
		return -1;
	}
	*data = q->bucket[q->rindex];
	q->rindex = next(q->rindex, q->max);
	q->nr--;
	q->de_count++;
	pthread_cond_signal(&q->cond_eq);
	pthread_mutex_unlock(&q->mut);

	return 0;
}

uint32_t queue_size(queue_t *ptr)
{
	struct queue_st *q=ptr;
	return q->nr;
}

/*
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
*/

int queue_delete(queue_t *ptr)
{
	struct queue_st *q=ptr;
	if (q == NULL) 
		return -1;
	pthread_mutex_destroy(&q->mut);
	pthread_cond_destroy(&q->cond_eq);
	pthread_cond_destroy(&q->cond_dq);
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
    cJSON_AddNumberToObject(result, "enqueue", q->en_count);
    cJSON_AddNumberToObject(result, "dequeue", q->de_count);

    return result;
}

