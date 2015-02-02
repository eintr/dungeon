/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "imp.h"
#include "util_misc.h"
#include "dungeon.h"
#include "util_log.h"
#include "util_atomic.h"
#include "thr_gravekeeper.h"

/** Global imp id serial generator. This is for debugging only, no critical usages. */
uint32_t global_imp_id___=1;
#define GET_NEXT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 1)

__thread imp_t *current_imp_=NULL;

int imp_timeout_cmp(void *p1, void *p2)
{
	imp_t *a=p1, *b=p2;

	return a->timeout_ms - b->timeout_ms;
}

int imp_ioevfd_cmp(void *p1, void *p2)
{
	imp_t *a=p1, *b=p2;

	return a->ioev_fd - b->ioev_fd;
}

static imp_t *imp_new(imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = calloc(1, sizeof(*imp));
    if (imp) {
        imp->id = GET_NEXT_IMP_ID;

//		imp->state = IMP_AWAKE;
//		imp->mut = MUTEX_UNLOCKED;

        imp->soul = soul;

        imp->memory = NULL;
    }
    return imp;
}

int imp_rip(imp_t *imp)
{
	imp->soul->fsm_delete(imp->memory);
	//mylog(L_DEBUG, "Deleted imp[%d].", imp->id);
	free(imp);
	atomic_decrease(&dungeon_heart->nr_total);
	return 0;
}

void imp_kill(imp_t *imp)
{
	imp->ioev_revents |= EV_MASK_KILL;
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
		mylog(L_INFO, "imp_new() failed.");
        return NULL;
    }
}

void imp_wake(imp_t *imp)
{
    queue_enqueue_nb(dungeon_heart->run_queue, imp);
}

static void imp_term(imp_t *imp)
{
	queue_enqueue_nb(dungeon_heart->grave_yard, imp);
	thr_gravekeeper_wakeup();
}

void imp_driver(imp_t *imp)
{
	int ret;
    struct epoll_event ev;

	mylog(L_DEBUG, "imp[%d] got ioev: 0x%.8lx\n", imp->id, imp->ioev_revents);
	if (imp->ioev_revents & EV_MASK_KILL) {
		mylog(L_DEBUG, "Imp[%d] was killed.\n", imp->id);
		imp_term(imp);
		return;
	}
	if (imp->ioev_revents & EV_MASK_GLOBAL_KILL) {
		mylog(L_DEBUG, "Imp[%d] was waken by alert_trap, terminate.\n", imp->id);
		imp_term(imp);
		return;
	}
	imp->ioev_events = 0;
	ret = imp->soul->fsm_driver(imp->memory);
	imp->ioev_revents = 0;
	switch (ret) {
		case TO_RUN:
			imp_wake(imp);
			break;
		case TO_BLOCK:
			mutex_lock(&dungeon_heart->index_mut);
			olist_add_entry(dungeon_heart->timeout_index, imp);
			mutex_unlock(&dungeon_heart->index_mut);

			if (imp->ioev_events != 0) {
				ev.data.ptr = imp;
				ev.events = imp->ioev_events | EPOLLRDHUP | EPOLLONESHOT;
				if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &ev)) {
					if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, imp->ioev_fd, &ev)) {
						mylog(L_ERR, "Imp[%d]: Can't register imp to dungeon_heart->epoll_fd, epoll_ctl(): %m, cancel it!\n", imp->id);
						abort();
					}
				}
			}

			thr_ioevent_interrupt();

			atomic_increase(&dungeon_heart->nr_waitio);
			break;
		case TO_TERM:
			imp_term(imp);
			break;
		case I_AM_NEO:
			/* This is for test. */
			break;
		default:
			mylog(L_ERR, "Imp[%d] returned bad code %d, this must be a BUG!\n", imp->id, ret);
			abort();
	}
}

int imp_set_ioev(int fd, uint32_t ev)
{
	current_imp_->ioev_fd = fd;
	current_imp_->ioev_events = ev | EPOLLONESHOT;
	return 0;
}

int imp_set_timer(int ms)
{
	if (ms<0) {
		current_imp_->timeout_ms = UINT32_MAX;
	} else {
		current_imp_->timeout_ms = systimestamp_ms() + ms;
	}
	return 0;
}

