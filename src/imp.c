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

__thread imp_t *current_imp_=NULL;

static imp_t *imp_new(imp_soul_t *soul)
{
    imp_t *imp = NULL;
	struct epoll_event epev;

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

        /** Register imp epoll_fd into dungeon_heart's epoll_fd */
        epev.events = 0;
        epev.data.ptr = imp;
        epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, imp->body->epoll_fd, &epev);
    }
    return imp;
}

int imp_dismiss(imp_t *imp)
{
	imp->soul->fsm_delete(imp->memory);
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

	if (dungeon_heart->nr_total >= dungeon_heart->nr_max) {
		mylog(L_INFO, "There are too many imps!");
		return NULL;
	}
    imp = imp_new(soul);
    if (imp) {
        imp->memory = memory;

        atomic_increase(&dungeon_heart->nr_total);

		imp->soul->fsm_new(imp->memory);

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

	//fprintf(stderr, "imp[%d] got ioev: 0x%.16llx\n", imp->id, imp->event_mask);
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
	imp->request_mask = 0;
	ret = imp->soul->fsm_driver(imp->memory);
	imp->event_mask = 0;
	switch (ret) {
		case TO_RUN:
			imp_wake(imp);
			break;
		case TO_BLOCK:
			ev.events = EPOLLIN|EPOLLOUT|EPOLLRDHUP|EPOLLONESHOT;
			ev.data.ptr = imp;
			if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->body->epoll_fd, &ev)) {
				mylog(L_WARNING, "Imp[%d]: Can't register imp to dungeon_heart->epoll_fd, epoll_ctl(): %m, cancel it!\n", imp->id);
				imp_term(imp);
			} else {
				atomic_increase(&dungeon_heart->nr_waitio);
			}
			break;
		case TO_TERM:
			imp_term(imp);
			break;
		case I_AM_NEO:
			/* DO nothing */
			break;
		default:
			mylog(L_ERR, "Imp[%d] returned bad code %d, this must be a BUG!\n", imp->id, ret);
			imp_term(imp);
			break;
	}
}

int imp_set_ioev(int fd, uint32_t ev)
{
	return imp_body_set_ioev(current_imp_->body, fd, ev);
}

int imp_set_timer(int ms)
{
	current_imp_->request_mask |= EV_MASK_TIMEOUT;
	return imp_body_set_timer(current_imp_->body, ms);
}

int imp_cancel_timer(imp_t *imp)
{
	return imp_body_set_timer(imp->body, 0);
}

uint64_t imp_get_ioev(imp_t *imp)
{
	uint64_t mask=0;

	if (imp->request_mask & EV_MASK_TIMEOUT) {
		if (imp_body_cleanup_timer(imp->body)) {
			mask |= EV_MASK_TIMEOUT;
			imp_cancel_timer(imp);
			mylog(L_DEBUG, "imp[%d] timeout.\n", imp->id);
		}
	}

	if (imp->request_mask & EV_MASK_EVENT) {
		mask |= EV_MASK_EVENT*imp_body_cleanup_event(imp->body);
		mylog(L_DEBUG, "imp[%d] gen_event.\n", imp->id);
	}

	mask |= imp_body_get_event(imp->body);
	return mask;
}

