/** \file thr_maintainer.c
	Maintainer thread consistansly collect those terminated imp's body.
*/

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/poll.h>
#include <errno.h>
/** \endcond */

#include "util_log.h"
#include "dungeon.h"
#include "imp.h"

static pthread_t tid;
static int event_fd;
static volatile int thread_quit_mark=0;

static void *thr_maintainer(void *p)
{
	imp_t *imp;
	sigset_t allsig;
	int i, err;
	struct pollfd pfd;
	uint64_t eventbuf;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	pfd.fd = event_fd;
	pfd.events = POLLIN;

	while (!thread_quit_mark) {
		/* deal nodes in terminated queue */
		do {
			for (i=0;;++i) {
				err = queue_dequeue_nb(dungeon_heart->terminated_queue, &imp);
				if (err < 0) {
					break;
				}
				mylog(L_DEBUG, "fetch context[%u] from terminate queue", imp->id);
				imp_dismiss(imp);
			}
		} while (i>0);
		if (poll(&pfd, 1, 1000)>0) {
			read(event_fd, &eventbuf, sizeof(eventbuf));
		}
	}

	return NULL;
}

/** Create maintainer thread */
int thr_maintainer_create(void)
{
	int err;

	event_fd = eventfd(0, 0);
	if (event_fd<0) {
		return errno;
	}
	err = pthread_create(&tid, NULL, thr_maintainer, NULL);
	if (err) {
		close(event_fd);
	}
	return err;
}

/** Delete maintainer thread */
int thr_maintainer_destroy(void)
{
	int err;
	thread_quit_mark = 1;
	err = pthread_join(tid, NULL);
	close(event_fd);
	return err;
}

/** Wake up the maintainer thread instantly */
void thr_maintainer_wakeup(void)
{
	/** \todo Prevent event_fd block */
	uint64_t event = 1ULL;
	write(event_fd, &event, sizeof(event));
}

