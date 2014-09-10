/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
/** \endcond */

#include "imp.h"
#include "dungeon.h"
#include "util_log.h"
#include "util_atomic.h"
#include "util_misc.h"
#include "thr_maintainer.h"

/** Global imp id serial generator. This is for debugging only, no critical usages. */
uint32_t global_imp_id___=1;
#define GET_NEXT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 1)

__thread imp_t *current_imp_=NULL;

static imp_t *imp_new(imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = malloc(sizeof(*imp));
    if (imp) {
        imp->id = GET_NEXT_IMP_ID;

		imp->event_mask = 0;

		imp->requested_events = llist_new(1000);
		imp->returned_events = llist_new(1000);

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
	struct epoll_data_st *msg;
	imp->soul->fsm_delete(imp->memory);

	while (llist_fetch_head_nb(imp->requested_events, &msg)==0) {
		free(msg);
	}
	llist_delete(imp->requested_events);

	while (llist_fetch_head_nb(imp->returned_events, &msg)==0) {
		free(msg);
	}
	llist_delete(imp->returned_events);

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

		imp->in_runq = IMP_NOT_RUNNING;

        imp_wake(imp);
		//mylog(L_DEBUG, "Summoned imp[%d].", imp->id);
        return imp;
    } else {
        return NULL;
    }
}

static void imp_run(imp_t *imp)
{
   	queue_enqueue_nb(dungeon_heart->run_queue, imp);
}

void imp_wake(imp_t *imp)
{
	if (atomic_cas(&imp->in_runq, IMP_NOT_RUNNING, IMP_RUNNING)) {
		atomic_decrease(&dungeon_heart->nr_waitio);
		imp_run(imp);
		mylog(L_DEBUG, "imp[%d] waked up.", imp->id);
	} else {
		mylog(L_DEBUG, "imp[%d] is already waken up.", imp->id);
	}
}

static void imp_term(imp_t *imp)
{
	queue_enqueue_nb(dungeon_heart->terminated_queue, imp);
	thr_maintainer_wakeup();
}

void imp_driver(imp_t *imp)
{
	unsigned int ret, count;
    struct epoll_event ev;
	intptr_t epoll_job;

	imp->event_mask = imp_reduct_event_mask(imp);
	mylog(L_DEBUG, "imp[%d] got ioev: 0x%.16llx\n", imp->id, imp->event_mask);
	if (imp->event_mask & EV_MASK_KILL) {
		mylog(L_DEBUG, "Imp[%d] was killed.\n", imp->id);
		imp->in_runq = IMP_NOT_RUNNING;
		imp_term(imp);
		return;
	}
	if (imp->event_mask & EV_MASK_GLOBAL_ALERT) {
		mylog(L_DEBUG, "Imp[%d] was waken by alert_trap, terminate.\n", imp->id);
		imp_term(imp);
		return;
	}
	ret = imp->soul->fsm_driver(imp->memory);
	imp->in_runq = IMP_NOT_RUNNING;		// TODO: ATOMIC HAZARD !!!!!!
	atomic_increase(&dungeon_heart->nr_waitio);

	if (ret==TO_TERM) {
		mylog(L_WARNING, "imp[%d]: Requested to termonate.", IMP_ID);
		imp_term(imp);
	} else {
		mylog(L_DEBUG, "imp[%d]: yielded.", imp->id);
		for (count=0; llist_fetch_head_nb(imp->requested_events, &epoll_job)==0; ++count) {
			switch (epoll_job) {
				case EV_MASK_TIMER:
					break;
				case EV_MASK_USER:
					break;
			}
			ev.data.ptr = epoll_job;
			ev.events = epoll_job->events;
			if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, epoll_job->fd, &ev)<0) {
				if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, epoll_job->fd, &ev)<0) {
					mylog(L_WARNING, "imp[%d]: Set epoll for fd %d failed: %m, TERMINATE!", IMP_ID, epoll_job->fd);
					imp_term(imp);
					return;
				}
			}
			mylog(L_DEBUG, "Imp[%d] is set to wait fd %d.\n", imp->id, epoll_job->fd);
		}
		if (count>0) {
			mylog(L_DEBUG, "Imp[%d] is sleeping to wait for %d fds.\n", imp->id, count);
		} else {
			mylog(L_DEBUG, "imp[%d]: No requested events, keep running.", IMP_ID);
			imp_wake(imp);
		}
	}
}

int imp_set_ioev(int fd, uint32_t ev)
{
	struct epoll_event ev;
/*	struct epoll_data_st *info;

	info=malloc(sizeof(*info));
	info->ev_mask = EV_MASK_USER;
	info->events = ev|EPOLLONESHOT;
	info->fd = fd;
	info->imp = current_imp_;

	mylog(L_DEBUG, "imp[%d]: imp_set_ioev(fd=%d)", current_imp_->id, fd);
*/
	return llist_append_nb(current_imp_->requested_events, EV_MASK_USER);
}

int imp_set_timer(int ms)
{
/*	struct epoll_data_st *info;

	info=malloc(sizeof(*info));
	info->ev_mask = EV_MASK_TIMER;
	info->events = EPOLLIN|EPOLLONESHOT;
	info->fd = current_imp_->body->timer_fd;
	info->imp = current_imp_;

	imp_body_set_timer(current_imp_->body, ms);
*/
	return llist_append_nb(current_imp_->requested_events, EV_MASK_TIMER);
}

int imp_cancel_timer(imp_t *imp)
{
	intptr_t get;

	imp_body_set_timer(imp->body, 0);
/*
	if (llist_fetch_match(imp->requested_events, NULL, EV_MASK_TIMER, &get)!=0) {
		mylog(L_DEBUG, "imp[%d]: imp_cancel_timer(): Not found in requested_events list.", imp->id);
	}
*/
	return 0;
}

uint64_t imp_reduct_event_mask(imp_t *imp)
{
	uint64_t mask=0;
	intptr_t msg;

	while (llist_fetch_head_nb(imp->returned_events, &msg)==0) {
		switch (msg->ev_mask) {
			case EV_MASK_TIMER:
				imp_cancel_timer(imp);
				imp_body_cleanup_timer(imp->body);
				mylog(L_DEBUG, "imp[%d] timeout.\n", imp->id);
				free(msg);
				break;
			case EV_MASK_EVENT:
				mask |= EV_MASK_EVENT;
				imp_body_cleanup_event(imp->body);
				mylog(L_DEBUG, "imp[%d] got generic event.\n", imp->id);
				free(msg);
				break;
			case EV_MASK_USER:
				mask |= EV_MASK_USER;
				free(msg);
				break;
			default:
				mylog(L_WARNING, "imp[%d] got unknown event.\n", imp->id);
				free(msg);
				break;
		}
	}

	return mask;
}

