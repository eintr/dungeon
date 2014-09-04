#include <stdio.h>

#include "ds_olist.h"
#include "util_timer.h"

struct timer_st {
	struct itimerspec expire_stamp_abs;
	timer_callback_func_t *cb;
	void *cb_arg;
};

static struct {
	olist_t *timer_list;
	int timerfd;
	pthread_t tid;
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
}


