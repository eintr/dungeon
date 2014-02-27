#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "proxy_pool.h"
#include "util_log.h"
#include "module_interface.h"

void *thr_maintainer(void *p)
{
	proxy_pool_t *pool=p;
	generic_context_t *node;
	sigset_t allsig;
	int i, err;
	struct timespec tv;

	tv.tv_sec = 2;
	tv.tv_nsec = 0;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!pool->maintainer_quit) {
		
		/* deal nodes in terminated queue */
		for (i=0;;++i) {
			err = llist_fetch_head_nb(pool->terminated_queue, (void **)&node);
			if (err < 0) {
				break;
			}
			mylog(L_DEBUG, "fetch context[%u] from terminate queue", node->id);
			node->module_iface->fsm_delete(node);
		}
		if (i==0) {
			//mylog(L_DEBUG, "%u : terminated_queue is empty.\n", gettid());
			// May be dynamically increase the sleep interval.
		} else {
			// May be dynamically decrease the sleep interval.
		}

		nanosleep(&tv, NULL);
	}

	return NULL;
}

