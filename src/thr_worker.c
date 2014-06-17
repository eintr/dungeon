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
	int i;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	worker_id = (int)p;

	while (!worker_quit) {
		/* deal node in run queue */
		while (1) {
			err = queue_dequeue_nb(dungeon_heart->run_queue, &node);
			if (err < 0) {
				//mylog(L_DEBUG, "run_queue is empty.");
				break;
			}
			//mylog(L_DEBUG, "Worker[%d]: fetched imp[%lu] from run queue\n", worker_id, node->id);
			imp_driver(node);
		}

		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, 1000);
		if (num<0) {
			mylog(L_ERR, "epoll_wait(): %m");
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			mylog(L_DEBUG, "[%d]: epoll_wait got %d/1000 events", worker_id, num);
			for (i=0;i<num;++i) {
				//mylog(L_DEBUG, "epoll: context[%u] has event %u", ((imp_t*)(ioev[i].data.ptr))->id, ioev[i].events);
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

