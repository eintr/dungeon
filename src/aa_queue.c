#include "aa_queue.h"

queue *queue_init(uint32_t max)
{
	queue *q;
	void **b;
	q = (queue *) malloc(sizeof(queue));
	if (q == NULL) {
		return NULL;
	}
	b = (void **) malloc(max * sizeof(void *));
	if (b == NULL) {
		free(q);
		return NULL;
	}
	q->bucket = b;
	q->max = max;
	q->rindex = q->windex = q->max_rindex = 0;
	return q;
}

int queue_enqueue(queue *q, void *data)
{
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
	
int queue_dequeue(queue *q, void **data)
{
	uint32_t max_rindex;
	uint32_t rindex;

	do {
		rindex = q->rindex;
		max_rindex = q->max_rindex;
		if ((rindex % q->max) == (max_rindex % q->max)) {
			return -1; // queue is empty
		}
		*data = q->bucket[rindex];
	} while(!CAS(&q->rindex, rindex, (rindex + 1) % q->max)); 

	return 0;
}

uint32_t queue_size(queue *q)
{
	if (q->windex >= q->rindex) {
		return q->windex - q->rindex;
	} else {
		return q->max + q->windex - q->rindex;
	}
}

void queue_travel(queue *q, travel_func tf)
{
	uint32_t idx;
	idx = q->rindex;
	while (idx != q->max_rindex) {
		tf(q->bucket[idx]);
		idx = (idx + 1) % q->max;
	}
}

int queue_destroy(queue *q)
{
	if (q == NULL) 
		return -1;
	if (q->bucket)
		free(q->bucket);
	free(q);
	return 0;
}

