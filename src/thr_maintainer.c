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
#include <sys/poll.h>
#include <errno.h>
/** \endcond */

#include "util_log.h"
#include "dungeon.h"
#include "imp.h"

static pthread_t tid;
static pthread_mutex_t mut_pending_event = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_pending_event = PTHREAD_COND_INITIALIZER;
static volatile int pending_event=0;
static volatile int thread_quit_mark=0;

/** Maintainer thread function */
static void *thr_maintainer(void *p)
{
	imp_t *imp;
	sigset_t allsig;
	int err;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!thread_quit_mark) {
		/* deal nodes in terminated queue */
		do {
			err = queue_dequeue_nb(dungeon_heart->terminated_queue, &imp);
			if (err < 0) {
				break;
			}
			//mylog(L_DEBUG, "fetch context[%u] from terminate queue", imp->id);
			imp_dismiss(imp);
		} while (1);
		pthread_mutex_lock(&mut_pending_event);
		while (pending_event==0) {
			pthread_cond_wait(&cond_pending_event, &mut_pending_event);
		}
		pending_event=0;
		pthread_mutex_unlock(&mut_pending_event);
	}

	return NULL;
}

/** Create maintainer thread */
int thr_maintainer_create(void)
{
	int err;

	err = pthread_create(&tid, NULL, thr_maintainer, NULL);
	if (err) {
		return err;
	}
	return 0;
}

/** Delete maintainer thread */
int thr_maintainer_destroy(void)
{
	int err;
	thread_quit_mark = 1;
	err = pthread_join(tid, NULL);
	return err;
}

/** Wake up the maintainer thread instantly */
void thr_maintainer_wakeup(void)
{
	pthread_mutex_lock(&mut_pending_event);
	pending_event = 1;
	pthread_cond_signal(&cond_pending_event);
	pthread_mutex_unlock(&mut_pending_event);
}

