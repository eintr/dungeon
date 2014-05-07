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

static void run_context(imp_t *imp)
{
	int ret;
	ret = imp->soul->fsm_driver(imp->memory);
	if (ret==TO_RUN) {
		imp_set_run(imp);
	} else if (ret==TO_WAIT_IO) {
		imp_set_iowait(imp->body->epoll_fd, imp);
	} else if (ret==TO_TERM) {
		imp_set_term(imp);
	} else {
		mylog(L_ERR, "FSM driver returned bad code %d, this must be a BUG!\n", ret);
		imp_set_term(imp);	// Terminate the BAD fsm.
	}
}

void *thr_worker(void *p)
{
	int worker_id;
	const int IOEV_SIZE=nr_cpus;
	imp_t *node;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int i;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	worker_id = (int)p;

	while (!dungeon_heart->worker_quit) {
		/* deal node in run queue */
		while (1) {
			err = llist_fetch_head_nb(dungeon_heart->run_queue, (void **)&node);
			if (err < 0) {
				//mylog(L_DEBUG, "run_queue is empty.");
				break;
			}
			mylog(L_DEBUG, "Worker[%d]: fetched imp[%lu] from run queue\n", worker_id, node->id);
			run_context(node);
		}

		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, 1000);
		if (num<0) {
			//mylog(L_ERR, "epoll_wait(): %s", strerror(errno));
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			for (i=0;i<num;++i) {
				mylog(L_DEBUG, "epoll: context[%u] has event %u", ((imp_t*)(ioev[i].data.ptr))->id, ioev[i].events);
				run_context(ioev[i].data.ptr);
			}
		}
	}

	return NULL;
}

