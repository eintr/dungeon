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
	int i, num;
	struct epoll_event ioev[IOEV_SIZE];
	imp_t *imp, *head;
	time_t now;
	int timeout;

	// TODO: raise_thread_prio(5);

	while (!terminate) {
		now = systimestamp_ms();
		mutex_lock(&dungeon_heart->index_mut);
		head = olist_peek_head(dungeon_heart->timeout_index);
		mutex_unlock(&dungeon_heart->index_mut);
        if (head==NULL) {
            timeout=-1;
        } else{
            timeout = head->timeout_ms - now;
			if (timeout<0) {
fprintf(stderr, "thr_ioevent: !!! Got Negetive timeout = %d-%d = %d.\n", head->timeout_ms, now, timeout);
				timeout=0;
			}
        }

		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, timeout);
		if (num<0) {
			if (errno!=EINTR) {
				mylog(L_ERR, "thr_ioevent(): epoll_wait(): %m");
			}
		} else if (num==0) {
			mutex_lock(&dungeon_heart->index_mut);
            while (1) {
                imp = olist_fetch_head(dungeon_heart->timeout_index);
                if (imp==NULL) {
                    break;
                }
                if (imp->timeout_ms < now) { /* Timed out */
                    imp->ioev_revents = EV_MASK_TIMEOUT;
					atomic_decrease(&dungeon_heart->nr_waitio);
                    imp_wake(imp);
                } else {
                    olist_add_entry(dungeon_heart->timeout_index, imp);    /* Feed non-timedout imp back. */
                    break;
                }
            }
			mutex_unlock(&dungeon_heart->index_mut);
		} else {
			mylog(L_DEBUG, "thr_ioevent(): epoll_wait got %d/%d events", num, IOEV_SIZE);
			for (i=0;i<num;++i) {
				imp = ioev[i].data.ptr;
				imp->ioev_revents = ioev[i].events;
				mutex_lock(&dungeon_heart->index_mut);
				olist_remove_entry(dungeon_heart->timeout_index, imp);
				mutex_unlock(&dungeon_heart->index_mut);
				atomic_decrease(&dungeon_heart->nr_waitio);
				imp_wake(imp);
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
	pthread_kill(tid, SIGUSR1);
}

