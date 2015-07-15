/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include "util_misc.h"
/** \endcond */

#include "imp.h"
#include "dungeon.h"
#include "util_log.h"
#include "util_atomic.h"
#include "ds_stack.h"
#include "thr_ioevent.h"

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

	imp = stack_pop(dungeon_heart->imp_cache);
	if (imp==NULL) {
    	imp = calloc(1, sizeof(*imp));
	}
    if (imp) {
		memset(imp, 0, sizeof(*imp));
        imp->id = GET_NEXT_IMP_ID;
        imp->soul = soul;
		imp->timeout_ms = TIMEOUT_MAX;
		imp->ioev_fd = -1;
    }
    return imp;
}

void imp_free(imp_t *imp)
{
	free(imp);
	atomic_decrease(&dungeon_heart->nr_total);
}

int imp_rip(imp_t *imp)
{
	imp->soul->fsm_delete(imp->memory);
	imp->memory = NULL;

	if (stack_push(dungeon_heart->imp_cache, imp)!=0) {
		imp_free(imp);
	}
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

	if (memory==NULL) {
		fprintf(stderr, "imp_summon() with memory==NULL!!\n");
	}
	if (dungeon_heart->nr_total >= dungeon_heart->nr_max) {
		mylog(L_INFO, "There are too many imps!");
		return NULL;
	}
    imp = imp_new(soul);
    if (imp) {
        imp->memory = memory;

        atomic_increase(&dungeon_heart->nr_total);

		imp->soul->fsm_new(imp->memory);

		queue_enqueue(dungeon_heart->run_queue, imp);
		//mylog(L_DEBUG, "Summoned imp[%d].", imp->id);
        return imp;
    } else {
		mylog(L_INFO, "imp_new() failed.");
        return NULL;
    }
}

static void imp_term(imp_t *imp)
{
	//mutex_lock(&dungeon_heart->index_mut);
	if (olist_remove_entry(dungeon_heart->timeout_index, imp)!=0) {
//fprintf(stderr, "Remove imp[%d] from index failed.\n", imp->id);
	}
	//mutex_unlock(&dungeon_heart->index_mut);
	imp_rip(imp);
}

void imp_driver(imp_t *imp)
{
	int ret;
    struct epoll_event ev;

	mylog(L_DEBUG, "imp[%d] got ioev: 0x%.8lx\n", imp->id, imp->ioev_revents);
	if (imp->ioev_revents & EV_MASK_KILL) {
		mylog(L_DEBUG, "Imp[%d] was been killed.\n", imp->id);
		imp_term(imp);
		return;
	}
	if (imp->ioev_revents & EV_MASK_GLOBAL_KILL) {
		mylog(L_DEBUG, "Imp[%d] was waken by alert_trap, terminate.\n", imp->id);
		imp_term(imp);
		return;
	}
run:
	current_imp_ = imp;
	imp->ioev_events = 0;
	imp->ioev_fd = -1;
	ret = imp->soul->fsm_driver(imp->memory);
	imp->ioev_revents = 0;
	switch (ret) {
		case I_AM_NEO:
			/* This is for test. */
			break;
		case TO_TERM:
			imp_term(imp);
			break;
		case TO_RUN:
		case TO_BLOCK:
			if (imp->ioev_events != 0 && imp->ioev_fd>=0) {
				//mutex_lock(&dungeon_heart->index_mut);
				if (olist_add_entry(dungeon_heart->timeout_index, imp)!=0) {
fprintf(stderr, "Failed to insert imp[%d] into timeout_index.\n", imp->id);
				}
				//mutex_unlock(&dungeon_heart->index_mut);
				ev.data.ptr = imp;
				ev.events = imp->ioev_events | EPOLLRDHUP | EPOLLONESHOT;
				if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &ev)) {
					if (epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, imp->ioev_fd, &ev)) {
						mylog(L_ERR, "Imp[%d]: Can't register imp to dungeon_heart->epoll_fd, epoll_ctl(): %m, cancel it!\n", imp->id);
						abort();
					}
				}
				thr_ioevent_interrupt();
				atomic_increase(&dungeon_heart->nr_waitio);
			} else {
				goto run;
				//queue_enqueue(dungeon_heart->run_queue, imp);
			}
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
		current_imp_->timeout_ms = systimestamp_ms() + TIMEOUT_MAX;
	} else {
		current_imp_->timeout_ms = systimestamp_ms() + (uint64_t)ms;
	}
	return 0;
}

