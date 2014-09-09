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
	uint32_t epoll_timeout_ms;
	uint32_t loop_count;
	int cpubind;
};

static struct worker_info_st *info_thr;
static int num_thr;
static pthread_t tid_epoll_thr;
static volatile int worker_quit=0;

const int IOEV_SIZE=10240;

static void *thr_epoller(void *p) {
	imp_t *imp;
	struct epoll_data_st *msg;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int num, i;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!worker_quit) {
		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, 1000);
		if (num<0) {
			mylog(L_ERR, "epoll_wait(): %m");
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			mylog(L_DEBUG, "epoll worker: epoll_wait got %d/%d events", num, IOEV_SIZE);
			for (i=0;i<num;++i) {
				msg = ioev[i].data.ptr;
				imp = msg->imp;
				imp_cancel_timer(imp);
				imp->event_mask |= msg->ev_mask;
				llist_append_nb(imp->returned_events, msg);
				mylog(L_DEBUG, "epoll worker: transfered msg of fd %d to returned_events.", msg->fd);
				imp_wake(imp);
			}
		}
	}

	pthread_exit(NULL);
}

/** Worker thread function */
static void *thr_driver(void *p)
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

int thr_worker_create(int num, cpu_set_t *cpuset)
{
	int err;
	int c;
	cpu_set_t thr_cpuset;
	pthread_attr_t attr;

	err = pthread_create(&tid_epoll_thr, NULL, thr_epoller, NULL);
	if (err) {
		mylog(L_WARNING, "pthread_create(thr_epoller) failed: %s.", strerror(err));
		return -1;
	}

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
		err = pthread_create(&info_thr[num_thr].tid, &attr, thr_driver, (void*)num_thr);
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
	pthread_join(tid_epoll_thr, NULL);
	mylog(L_DEBUG, "epoll_thread exited.");
	for (i=0;i<num_thr;++i) {
		pthread_cancel(info_thr[i].tid);
	}
	for (i=0;i<num_thr;++i) {
		pthread_join(info_thr[i].tid, NULL);
	}
	mylog(L_DEBUG, "worker_threads exited.");
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
		cJSON_AddNumberToObject(item, "EPOLLTimeOut_ms", info_thr[i].epoll_timeout_ms);
		cJSON_AddNumberToObject(item, "CPUBind", info_thr[i].cpubind);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

