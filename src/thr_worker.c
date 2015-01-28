#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "dungeon.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "util_atomic.h"
#include "imp.h"

struct worker_info_st {
	pthread_t tid;
	uint32_t loop_count;
	int cpubind;
};

static struct worker_info_st *info_thr;
static int num_thr;
static volatile int worker_quit=0;

const int IOEV_SIZE=10240;

/** Worker thread function */
static void *thr_worker(void *p)
{
	intptr_t worker_id;
	imp_t *imp;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int i, r;
	int queue_burst;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	worker_id = (intptr_t)p;

	while (!worker_quit) {
		if (queue_dequeue_nb(dungeon_heart->run_queue, &imp)<0) {
			sched_yield();
			continue;
		}
		atomic_increase(&info_thr[worker_id].loop_count);
		current_imp_ = imp;
		imp_driver(imp);
	}

	return NULL;
}

int thr_worker_create(int num, cpu_set_t *cpuset)
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
		info_thr[num_thr].loop_count = 0;
		err = pthread_create(&info_thr[num_thr].tid, &attr, thr_worker, (void*)num_thr);
		if (err) {
			break;
		}
		pthread_attr_destroy(&attr);
	}
	return num_thr;
}

int thr_worker_destroy(void)
{
	int i;

	worker_quit = 1;
	for (i=0;i<num_thr;++i) {
		pthread_join(info_thr[i].tid, NULL);
	}
	return 0;
}

cJSON *thr_worker_serialize(void)
{
	int i;
	cJSON *result, *item;

	result = cJSON_CreateArray();
	for (i=0;i<num_thr;++i) {
		item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "WorkerID", i);
		cJSON_AddNumberToObject(item, "LoopCount", info_thr[i].loop_count);
		cJSON_AddNumberToObject(item, "CPUBind", info_thr[i].cpubind);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

