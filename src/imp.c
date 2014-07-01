/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "imp.h"
#include "dungeon.h"
#include "util_log.h"
#include "util_atomic.h"
#include "thr_maintainer.h"

/** Global imp id serial generator. This is for debugging only, no critical usages. */
uint32_t global_imp_id___=1;
#define GET_NEXT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 1)

static imp_t *imp_new(imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = malloc(sizeof(*imp));
    if (imp) {
        imp->id = GET_NEXT_IMP_ID;

		imp->request_mask = 0;
		imp->event_mask = 0;

        imp->body = imp_body_new();
		if (imp->body == NULL) {
			free(imp);
			return NULL;
		}
        imp->soul = soul;

        imp->memory = NULL;
    }
    return imp;
}

int imp_dismiss(imp_t *imp)
{
	imp->soul->fsm_delete(imp);
	imp_body_delete(imp->body);
	imp->body = NULL;
	//mylog(L_DEBUG, "Deleted imp[%d].", imp->id);
	free(imp);
	atomic_decrease(&dungeon_heart->nr_total);
	return 0;
}

void imp_kill(imp_t *imp)
{
	imp->event_mask |= EV_MASK_KILL;
}

imp_t *imp_summon(void *memory, imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = imp_new(soul);
    if (imp) {
		imp->request_mask = 0;
		imp->event_mask = 0;

        imp->memory = memory;

        atomic_increase(&dungeon_heart->nr_total);

		imp->soul->fsm_new(imp);

        imp_wake(imp);
		//mylog(L_DEBUG, "Summoned imp[%d].", imp->id);
        return imp;
    } else {
        return NULL;
    }
}

void imp_wake(imp_t *imp)
{
    queue_enqueue_nb(dungeon_heart->run_queue, imp);
}

static void imp_term(imp_t *imp)
{
	queue_enqueue_nb(dungeon_heart->terminated_queue, imp);
	thr_maintainer_wakeup();
}

void imp_driver(imp_t *imp)
{
	int ret;
    struct epoll_event ev;

	if (imp->request_mask & EV_MASK_TIMEOUT) {	// Stop timer if imp_set_timer() before
		imp_cancel_timer(imp);
	}
	imp->event_mask = imp_get_ioev(imp);
	if (imp->event_mask & EV_MASK_KILL) {
		mylog(L_DEBUG, "Imp[%d] was killed.\n", imp->id);
		imp_term(imp);
		return;
	}
	if (imp->event_mask & EV_MASK_GLOBAL_ALERT) {
		mylog(L_DEBUG, "Imp[%d] was waken by alert_trap, terminate.\n", imp->id);
		imp_term(imp);
		return;
	}
	if (imp->event_mask & EV_MASK_TIMEOUT) {
		imp_body_cleanup_timer(imp->body);
	}
	if (imp->event_mask & EV_MASK_EVENT) {
		imp_body_cleanup_event(imp->body);
	}
	imp->request_mask = 0;
	ret = imp->soul->fsm_driver(imp);
	switch (ret) {
		case TO_RUN:
			imp_wake(imp);
			break;
		case TO_WAIT_IO:
			ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLONESHOT;
			ev.data.ptr = imp;
			epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->body->epoll_fd, &ev);
			atomic_increase(&dungeon_heart->nr_waitio);
			break;
		case TO_TERM:
			imp_term(imp);
			break;
		default:
			mylog(L_ERR, "Imp[%d] returned bad code %d, this must be a BUG!\n", imp->id, ret);
			imp_term(imp);
			break;
	}
}


int imp_set_ioev(imp_t *imp, int fd, uint32_t ev)
{
	return imp_body_set_ioev(imp->body, fd, ev);
}

uint64_t imp_get_ioev(imp_t *imp)
{
	return imp_body_get_event(imp->body);
}

int imp_set_timer(imp_t *imp, int ms)
{
	imp->request_mask |= EV_MASK_TIMEOUT;
	return imp_body_set_timer(imp->body, ms);
}

int imp_cancel_timer(imp_t *imp)
{
	return imp_body_set_timer(imp->body, 0);
}

