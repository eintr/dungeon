#include <stdio.h>
#include <time.h>

#include "util_misc.h"

#define	CONST_NANOSEC_1sec	(1*1000*1000*1000)

void timespec_normalize(struct timespec *ts)
{
	if (ts->tv_nsec > CONST_NANOSEC_1sec) {
		ts->tv_sec += ts->tv_nsec/CONST_NANOSEC_1sec;
		ts->tv_nsec %= CONST_NANOSEC_1sec;
	} else if (ts->tv_nsec < 0) {
		ts->tv_sec -= ts->tv_nsec/CONST_NANOSEC_1sec;
		ts->tv_nsec = (ts->tv_nsec%CONST_NANOSEC_1sec)+CONST_NANOSEC_1sec;
	}
}

struct timespec timespec_now(struct timespec *p)
{
	struct timespec res;

	clock_gettime(CLOCK_MONOTONIC, &res);
	if (p!=NULL) {
		memcpy(&res, p, sizeof(struct timespec));
	}
	return res;
}

struct timespec timespec_sub(struct timespec *ts1, struct timespec *ts2)
{
	struct timespec res;
	res.tv_sec = ts1->tv_sec - ts2->tv_sec;
	res.tv_nsec = ts1->tv_nsec - ts2->tv_nsec;
	timespec_normalize(&res);
	return res;
}

struct timespec timespec_add(struct timespec *ts1, struct timespec *ts2)
{
	struct timespec res;
	res.tv_sec = ts1->tv_sec + ts2->tv_sec;
	res.tv_nsec = ts1->tv_nsec + ts2->tv_nsec;
	timespec_normalize(&res);
	return res;
}

struct timespec timespec_add_ms(struct timespec *ts1, int ms)
{
	struct timespec res;
	res.tv_sec = ts1->tv_sec;
	res.tv_nsec = ts1->tv_nsec + ms*1000*1000;
	timespec_normalize(&res);
	return res;
}

struct timespec timespec_by_ms(int ms)
{
	struct timespec res;
	res.tv_sec = ms/1000;
	res.tv_nsec = ms%1000*1000*1000;
	return res;
}

