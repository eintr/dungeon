#include <stdio.h>
#include <stdlib.h>
#include <timer.h>
#include <pthread.h>

#include "ds_olist.h"
#include "util_timer.h"

#define	SIG_TIMER_REQ	SIGUSR1

struct timer_st {
	struct itimerspec expire_stamp_abs;
	timer_callback_func_t *cb;
	void *cb_arg;
	int status;
};

static struct {
	olist_t *timer_list;
	pthread_t tid;
	queue_t *pending_requests;
} timer_facility;

static int cmp_itimerspec(const olist_data_t *p1, const olist_data_t *p2)
{
	struct timer_st *a=p1->data, *b=p2->data;
	time_t delta_sec;
	long delta_nsec;

	delta_sec = a->expire_stamp_abs.tv_sec - b->expire_stamp_abs.tv_sec;
	if (delta_sec) {
		return delta_sec;
	}
	delta_nsec = a->expire_stamp_abs.tv_nsec - b->expire_stamp_abs.tv_nsec;
	if (delta_nsec) {
		return delta_nsec;
	}
	return 0;
}

static void timespec_normalize(struct timespec *ts)
{
	// TODO
}

static struct timespec timespec_sub(struct timespec *ts1, struct timespec *ts2)
{
	// TODO
	timespec_normalize(&t->expire_stamp_abs);
}

static struct timespec timespec_add(struct timespec *ts1, struct timespec *ts2)
{
	// TODO
	timespec_normalize(&t->expire_stamp_abs);
}

static struct timespec timespec_add_ms(struct timespec *ts1, int ms)
{
	// TODO
	timespec_normalize(&t->expire_stamp_abs);
}

static void pending_timers_enqueue(void)
{
	// TODO
}

static void *thr_timer(void *p)
{
	// TODO
	struct timer_st *timer0;
	struct timespec tv;

	sigaction();

	pthread_sigmask();

	while (1) {
		pending_timers_enqueue();
		timer0 = olist_get_minimal();
		if (list is empty) {
			sigsuspend();
			continue;
		}
		tv = timespec_sub(timer0, NOW);
		if (tv<=0) {
			timer0 = olist_fetch_minimal();
			callback();
			free(timer0);
			continue;
		}
		if (nanosleep(tv) is Over) {
			timer0 = olist_fetch_minimal();
			callback();
			free(timer0);
		}
	}
	pthread_sigmask();
	pthread_exit(NULL);
}

int timer_facility_init(void)
{
	timer_facility.timer_list = olist_new(100000, cmp_itimerspec);
	if (timer_facility.timer_list == NULL) {
		return -1;
	}
	return 0;
}

int timer_facility_destroy(void)
{
	olist_delete(timer_facility.timer_list);
	timer_facility.timer_list = NULL;
	close(timer_facility.timer_fd);
	timer_facility.timer_fd=-1;
}

void* timer_set_rel(int ms, timer_callback_func_t *cb, void *cb_arg)
{
	struct timer_st *t;

	t = malloc(*t);
	if (t==NULL) {
		return NULL;
	}

	clock_gettime(CLOCK_MONOTONIC, &t->expire_stamp_abs);
	t->expire_stamp_abs = timespec_add_ms(&t->expire_stamp_abs, ms);
	t->cb = cb;
	t->cb_arg = cb_arg;
	t->status = TIMER_OK;

	if (queue_enqueue_nb(&timer_facility.pending_requests, t)) {
		free(t);
		return NULL;
	}
	pthread_kill(timer_facility.tid, SIG_TIMER_REQ);

	return t;	
}

int timer_cancel(void*);
int timer_drop(void*);

