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

extern int nr_cpus;

struct worker_info_st {
	pthread_t tid;
	uint32_t run_weight;
	uint32_t epoll_timeout_ms;
	uint32_t loop_count;
};

static struct worker_info_st *info_thr;
static int num_thr;
static volatile int worker_quit=0;

const int IOEV_SIZE=1024;

/** Worker thread function */
static void *thr_worker(void *p)
{
	int worker_id;
	imp_t *node;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int i, r;
	struct timeval t0, t1, dt;
	int dt_ms;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	worker_id = (int)p;

	//rweight = info_thr[worker_id].run_weight;
	//epoll_timeout_ms = info_thr[worker_id].epoll_timeout_ms;

	while (!worker_quit) {
		atomic_increase(&info_thr[worker_id].loop_count);
		//fprintf(stderr, "rweight=%d,\tepoll_timeout_ms=%d            \r", rweight, epoll_timeout_ms);
		/* deal node in run queue */
		gettimeofday(&t0, NULL);
		for (r=0; r<info_thr[worker_id].run_weight; ++r) {
			err = queue_dequeue_nb(dungeon_heart->run_queue, &node);
			if (err < 0) {
				//mylog(L_DEBUG, "run_queue is empty.");
				break;
			}
			imp_driver(node);
		}
		gettimeofday(&t1, NULL);

		/** If runqueue is too busy, reduce rweight and epoll_timeout to avoid connection block */
		timersub(&t1, &t0, &dt);
		dt_ms = dt.tv_sec*1000+dt.tv_usec/1000;
		if (dt_ms > 100) {
			info_thr[worker_id].run_weight = (int)((double)info_thr[worker_id].run_weight*100.0F/(double)dt_ms);
			info_thr[worker_id].epoll_timeout_ms = (int)(info_thr[worker_id].epoll_timeout_ms*0.618F);
		}

		/** If runqueue is too idle, increase block timeout to supress CPU load */
		if (r==0) {
			info_thr[worker_id].epoll_timeout_ms = (int)(info_thr[worker_id].epoll_timeout_ms*1.618F);
			if (info_thr[worker_id].epoll_timeout_ms>1000) {
				info_thr[worker_id].epoll_timeout_ms=1000;	/** But never block longer than 1s */
			}
			info_thr[worker_id].run_weight = (int)(info_thr[worker_id].run_weight*1.618F);
			if (info_thr[worker_id].run_weight>100000) {
				info_thr[worker_id].run_weight=100000;
			}
		}

		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, info_thr[worker_id].epoll_timeout_ms);
		if (num<0) {
			mylog(L_ERR, "epoll_wait(): %m");
		} else if (num==0) {
			/** If IO is too idle, increase block timeout to supress CPU load */
			info_thr[worker_id].epoll_timeout_ms = (int)(info_thr[worker_id].epoll_timeout_ms*1.618F);
			if (info_thr[worker_id].epoll_timeout_ms>1000) {
				info_thr[worker_id].epoll_timeout_ms=1000;	/** But never block longer than 1s */
			}
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			//mylog(L_DEBUG, "Worker[%d]: epoll_wait got %d/1000 events", worker_id, num);
			for (i=0;i<num;++i) {
				imp_wake(ioev[i].data.ptr);
				atomic_decrease(&dungeon_heart->nr_waitio);
			}
		}
	}

	return NULL;
}

int thr_worker_create(int num)
{
	int err;

	info_thr = malloc(num*sizeof(struct worker_info_st));
	if (info_thr==NULL) {
		mylog(L_WARNING, "malloc(): %m.");
		return -1;
	}
	for (num_thr=0; num_thr<num; ++num_thr) {
		info_thr[num_thr].run_weight = 10000;
		info_thr[num_thr].epoll_timeout_ms = 1000;
		info_thr[num_thr].loop_count = 0;
		err = pthread_create(&info_thr[num_thr].tid, NULL, thr_worker, (void*)num_thr);
		if (err) {
			break;
		}
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
		cJSON_AddNumberToObject(item, "RunWeight", info_thr[i].run_weight);
		cJSON_AddNumberToObject(item, "EPOLLTimeOut_ms", info_thr[i].epoll_timeout_ms);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

