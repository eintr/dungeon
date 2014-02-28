#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "dungeon.h"
#include "util_log.h"
#include "room_template.h"
#include "imp.h"

void *thr_maintainer(void *p)
{
	dungeon_t *pool=p;
	imp_t *node;
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
			node->soul->fsm_delete(node);
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

