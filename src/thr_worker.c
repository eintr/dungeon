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
#include "room_template.h"
#include "imp.h"

extern int nr_cpus;

static void run_context(dungeon_t* pool, imp_t *imp)
{
	int ret;
	ret = imp->soul->fsm_driver(imp);
	if (ret==TO_RUN) {
		imp_set_run(pool, imp);
	} else if (ret==TO_WAIT_IO) {
		imp_set_iowait(pool, imp->body->epoll_fd, imp);
	} else if (ret==TO_TERM) {
		imp_set_term(pool, imp);
	} else {
		mylog(L_ERR, "FSM driver returned bad code %d, this must be a BUG!\n", ret);
		imp_set_term(pool, imp);	// Terminate the BAD fsm.
	}
}

void *thr_worker(void *p)
{
	const int IOEV_SIZE=nr_cpus;
	dungeon_t *pool=p;
	imp_t *node;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int i;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!pool->worker_quit) {
		/* deal node in run queue */
		while (1) {
			err = llist_fetch_head_nb(pool->run_queue, (void **)&node);
			if (err < 0) {
				//mylog(L_DEBUG, "run_queue is empty.");
				break;
			}
			mylog(L_DEBUG, "fetch from run queue node is %p\n", node);
			run_context(pool, node);
		}

		num = epoll_wait(pool->epoll_fd, ioev, IOEV_SIZE, 1000);
		if (num<0) {
			//mylog(L_ERR, "epoll_wait(): %s", strerror(errno));
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			for (i=0;i<num;++i) {
				mylog(L_DEBUG, "epoll: context[%u] has event %u", ((imp_t*)(ioev[i].data.ptr))->id, ioev[i].events);
				run_context(pool, ioev[i].data.ptr);
			}
		}
	}

	return NULL;
}

