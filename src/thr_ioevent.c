#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include "thr_ioevent.h"
#include "util_log.h"
#include "imp.h"
#include "dungeon.h"

static pthread_t tid;
static int terminate = 0;

static const int IOEV_SIZE=10240;

void *thr_ioevent(void *unused)
{
	int i, num;
	struct epoll_event null_ev, ioev[IOEV_SIZE];
	imp_t *imp;

	// TODO: raise_thread_prio(5);

	null_ev.data.ptr = NULL;
	null_ev.events = 0;

	while (!terminate) {
		num = epoll_wait(dungeon_heart->epoll_fd, ioev, IOEV_SIZE, -1);
		if (num<0) {
			mylog(L_ERR, "thr_ioevent(): epoll_wait(): %m");
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			mylog(L_DEBUG, "thr_ioevent(): epoll_wait got %d/%d events", num, IOEV_SIZE);
			for (i=0;i<num;++i) {
				imp = ioev[i].data.ptr;
				epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &null_ev);
				epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->body->timer_fd, &null_ev);
				imp_wake(imp);
			}
		}
	}
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


