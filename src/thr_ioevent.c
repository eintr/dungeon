/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
/** \endcond */

#include "thr_ioevent.h"
#include "util_atomic.h"
#include "util_log.h"
#include "util_misc.h"
#include "dungeon.h"

/** Thread id of io event monitor, only 1 thread is needed for a process */
static pthread_t tid;

static int terminate = 0;

static const int IOEV_SIZE=10240;

/**	Thread function of io event monitor */
void *thr_ioevent(void *unused)
{
	int i, num, count;
	struct epoll_event ioev[IOEV_SIZE], null_ev;
	imp_t *imp, *head;
	uint64_t now;
	int timeout;

	// TODO: raise_thread_prio(5);

	null_ev.events = 0;
	null_ev.data.ptr = NULL;

	while (!terminate) {
		now = systimestamp_ms();

		//mutex_lock(&dungeon_heart->index_mut);
		head = olist_peek_head(dungeon_heart->timeout_index);
		//mutex_unlock(&dungeon_heart->index_mut);
        if (head==NULL) {
            timeout=999;
        } else{
            timeout = head->timeout_ms - now;
			if (timeout<0) {
fprintf(stderr, "thr_ioevent: imp[%d] has timedout -> %llu-%llu = %d.\n", head->id, head->timeout_ms, now, timeout);
				timeout=0;
			}
        }

		if (timeout>0) {
			num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, timeout);
			if (num<0 && errno!=EINTR) {
				mylog(L_ERR, "thr_ioevent(): epoll_wait(): %m");
			}
		} else {
fprintf(stderr, "thr_ioevent: epoll_wait() ignored.\n");
			num = 0;
		}

		count=0;
		while (1) {
			//mutex_lock(&dungeon_heart->index_mut);
			imp = olist_fetch_head_nb(dungeon_heart->timeout_index);
			//mutex_unlock(&dungeon_heart->index_mut);
			if (imp==NULL) {
				break;
			}
			if (imp->timeout_ms < now) { /* Timed out */
fprintf(stderr, "imp[%u] timed out(%llu < %llu).\n", imp->id, imp->timeout_ms, now);
				imp->ioev_revents = EV_MASK_TIMEOUT;
				atomic_decrease(&dungeon_heart->nr_waitio);
				epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &null_ev);
				queue_enqueue(dungeon_heart->run_queue, imp);
				++count;
			} else {
				//mutex_lock(&dungeon_heart->index_mut);
				if (olist_add_entry(dungeon_heart->timeout_index, imp)!=0) {	/* Feed non-timedout imp back. */
fprintf(stderr, "Failed to feed imp[%d] back to timeout_index.\n");
abort();
				}
				//mutex_unlock(&dungeon_heart->index_mut);
				break;
			}
		}

//fprintf(stderr, "%d imps timed out.\n", count);

		for (i=0;i<num;++i) {
			imp = ioev[i].data.ptr;
			imp->ioev_revents = ioev[i].events;
			//mutex_lock(&dungeon_heart->index_mut);
			if (olist_remove_entry(dungeon_heart->timeout_index, imp)!=0) {
fprintf(stderr, "Remove imp[%d] from timeout_index {", imp->id);
olist_foreach(dungeon_heart->timeout_index, imp, {
	fprintf(stderr, "imp[%d],", imp->id);
});
fprintf(stderr, "} failed!\n");
abort();
			}
			//mutex_unlock(&dungeon_heart->index_mut);
			atomic_decrease(&dungeon_heart->nr_waitio);
			if (imp->memory==NULL) {
				fprintf(stderr, "thr_ioevent: !! Got imp[%d] with memory==NULL!\n", imp->id);
				abort();
			} else {
				queue_enqueue(dungeon_heart->run_queue, imp);
			}
		}
	}
	pthread_exit(NULL);
}

int thr_ioevent_init(void)
{
	int err;

	err = pthread_create(&tid, NULL, thr_ioevent, NULL);
	if (err) {
		mylog(L_ERR, "Failed to create thr_ioevent: %m");
		return -1;
	}

	return 0;
}

void thr_ioevent_destroy(void)
{
	pthread_cancel(tid);
	pthread_join(tid, NULL);
}

void thr_ioevent_interrupt(void)
{
//	pthread_kill(tid, SIGUSR1);
}

