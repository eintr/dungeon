#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <sched.h>
#include <time.h>

#include "dungeon.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "util_atomic.h"
#include "imp.h"

struct worker_info_st {
	pthread_t tid;
	int cpubind;
};

static struct worker_info_st *info_thr;
static int num_thr;
static volatile int scheduler_term=0;

/** Worker thread function */
static void *thr_scheduler(void *p)
{
	intptr_t worker_id;
	imp_t *imp;
	int err;
	sigset_t allsig;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	worker_id = (intptr_t)p;

	while (!worker_quit) {
		/* deal node in run queue */
		err = queue_dequeue(dungeon_heart->run_queue, &imp);
		if (err == 0) {
			current_imp_ = imp;
			mylog(L_DEBUG, "Fetched imp[%d] from run queue, push it.", imp->id);
			imp_driver(imp);
			mylog(L_DEBUG, "imp[%d] yielded.", imp->id);
			current_imp_ = NULL;
		}
	}

	return NULL;
}

int thr_scheduler_create(int num, cpu_set_t *cpuset)
{
	int err;
	int c;
	cpu_set_t thr_cpuset;
	pthread_attr_t attr;

	info_thr = malloc(num*sizeof(struct worker_info_st));
	if (info_thr==NULL) {
		mylog(L_WARNING, "malloc(): %m.");
		return -1;
	}

	c=0;
	for (num_thr=0; num_thr<num; ++num_thr) {
		for (;c<CPU_SETSIZE && !CPU_ISSET(c, &dungeon_heart->process_cpuset); ++c);
		if (CPU_ISSET(c, &dungeon_heart->process_cpuset)) {
			info_thr[num_thr].cpubind = c;
		}
		++c;
		if (c==CPU_SETSIZE) {
			c = CPU_SETSIZE-1;
		}
	}
	for (num_thr=0; num_thr<num; ++num_thr) {
		pthread_attr_init(&attr);
		CPU_ZERO(&thr_cpuset);
		CPU_SET(info_thr[num_thr].cpubind, &thr_cpuset);
		pthread_attr_setaffinity_np(&attr, sizeof(thr_cpuset), &thr_cpuset);
		info_thr[num_thr].epoll_timeout_ms = 1000;
		info_thr[num_thr].loop_count = 0;
		err = pthread_create(&info_thr[num_thr].tid, &attr, thr_scheduler, (void*)num_thr);
		if (err) {
			break;
		}
		pthread_attr_destroy(&attr);
	}
	return num_thr;
}

int thr_scheduler_destroy(void)
{
	int i;

	worker_quit = 1;
	pthread_join(tid_epoll_thr, NULL);
	mylog(L_DEBUG, "epoll_thread exited.");
	for (i=0;i<num_thr;++i) {
		pthread_cancel(info_thr[i].tid);
	}
	for (i=0;i<num_thr;++i) {
		pthread_join(info_thr[i].tid, NULL);
	}
	free(info_thr);
	mylog(L_DEBUG, "worker_threads exited.");
	return 0;
}

cJSON *thr_scheduler_serialize(void)
{
	int i;
	cJSON *result, *item;

	result = cJSON_CreateArray();
	for (i=0;i<num_thr;++i) {
		item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "SchedulerID", i);
		cJSON_AddNumberToObject(item, "CPUBind", info_thr[i].cpubind);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

