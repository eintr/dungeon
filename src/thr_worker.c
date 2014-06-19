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
#include "imp.h"

extern int nr_cpus;

static pthread_t *id_thr;
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
	int i, r, rweight;
	struct timeval t0, t1, dt;
	int epoll_timeout_ms, dt_ms;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	worker_id = (int)p;

	rweight=100;
	epoll_timeout_ms = 10;

	while (!worker_quit) {
		//fprintf(stderr, "rweight=%d,\tepoll_timeout_ms=%d            \r", rweight, epoll_timeout_ms);
		/* deal node in run queue */
		gettimeofday(&t0, NULL);
		for (r=0; r<rweight; ++r) {
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
			rweight = (int)((double)rweight*100.0F/(double)dt_ms);
			epoll_timeout_ms = (int)(epoll_timeout_ms*0.618F);
		}

		/** If runqueue is too idle, increase block timeout to supress CPU load */
		if (r==0) {
			epoll_timeout_ms = (int)(epoll_timeout_ms*1.618F);
			if (epoll_timeout_ms>1000) {
				epoll_timeout_ms=1000;	/** But never block longer than 1s */
			}
			rweight = (int)(rweight*1.618F);
			if (rweight>100000) {
				rweight=100000;
			}
		}

		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, epoll_timeout_ms);
		if (num<0) {
			mylog(L_ERR, "epoll_wait(): %m");
		} else if (num==0) {
			/** If IO is too idle, increase block timeout to supress CPU load */
			epoll_timeout_ms = (int)(epoll_timeout_ms*1.618F);
			if (epoll_timeout_ms>1000) {
				epoll_timeout_ms=1000;	/** But never block longer than 1s */
			}
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			//mylog(L_DEBUG, "Worker[%d]: epoll_wait got %d/1000 events", worker_id, num);
			for (i=0;i<num;++i) {
				imp_wake(ioev[i].data.ptr);
			}
		}
	}

	return NULL;
}

int thr_worker_create(int num)
{
	int err;

	id_thr = malloc(num*sizeof(pthread_t));
	if (id_thr==NULL) {
		mylog(L_WARNING, "malloc(): Not enough memory.");
		return -1;
	}
	for (num_thr=0; num_thr<num; ++num_thr) {
		err = pthread_create(id_thr+num_thr, NULL, thr_worker, (void*)num_thr);
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
		pthread_join(id_thr[i], NULL);
	}
	return 0;
}

