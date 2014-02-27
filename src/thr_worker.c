#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "imp_pool.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "module_interface.h"

extern int nr_cpus;

static void run_context(imp_pool_t* pool, generic_context_t *node)
{
	int ret;
	ret = node->module_iface->fsm_driver(node);
	if (ret==TO_RUN) {
		imp_set_run(pool, node);
	} else if (ret==TO_WAIT_IO) {
		imp_set_iowait(pool, node->epoll_fd, node);
	} else if (ret==TO_TERM) {
		imp_set_term(pool, node);
	} else {
		mylog(L_ERR, "FSM driver returned bad code %d, this must be a BUG!\n", ret);
		imp_set_term(pool, node);	// Terminate the BAD fsm.
	}
}

void *thr_worker(void *p)
{
	const int IOEV_SIZE=nr_cpus;
	imp_pool_t *pool=p;
	generic_context_t *node;
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
				mylog(L_DEBUG, "epoll: context[%u] has event %u", ((generic_context_t*)(ioev[i].data.ptr))->id, ioev[i].events);
				run_context(pool, ioev[i].data.ptr);
			}
		}
	}

	return NULL;
}

