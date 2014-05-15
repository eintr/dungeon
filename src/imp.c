/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
/** \endcond */

#include "imp.h"
#include "dungeon.h"
#include "util_log.h"
#include "util_atomic.h"

/** Global imp id serial generator */
uint32_t global_imp_id___=1;
#define GET_NEXT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 1)

static imp_t *imp_new(imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = malloc(sizeof(*imp));
    if (imp) {
        imp->id = GET_NEXT_IMP_ID;

        imp->body = imp_body_new();
        imp->soul = soul;

        imp->memory = NULL;
    }
    return imp;
}

int imp_dismiss(imp_t *imp)
{
	imp->soul->fsm_delete(imp->memory);
	mylog(L_DEBUG, "Deleting imp[%d]->body.", imp->id);
	imp_body_delete(imp->body);
	mylog(L_DEBUG, "Deleting imp[%d].", imp->id);
	free(imp);
	return 0;
}

imp_t *imp_summon(void *memory, imp_soul_t *soul)
{
    imp_t *imp = NULL;

    imp = imp_new(soul);
    if (imp) {
        imp->memory = memory;

        atomic_increase(&dungeon_heart->nr_total);

		imp->soul->fsm_new(memory);

        imp_wake(imp);
        return imp;
    } else {
        return NULL;
    }
}

void imp_wake(imp_t *imp)
{
    queue_enqueue_nb(dungeon_heart->run_queue, imp);
}

void imp_driver(imp_t *imp)
{
	int ret;
    struct epoll_event ev;

	if (imp->kill_mark) {
		mylog(L_DEBUG, "Imp[%d] was killed.\n", imp->id);
   		queue_enqueue_nb(dungeon_heart->terminated_queue, imp);
	} else {
		ret = imp->soul->fsm_driver(imp->memory);
		switch (ret) {
			case TO_RUN:
				imp_wake(imp);
				break;
			case TO_WAIT_IO:
				ev.events = EPOLLIN|EPOLLONESHOT;
				ev.data.ptr = imp;
				epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, imp->body->epoll_fd, &ev);
				break;
			case TO_TERM:
    			queue_enqueue_nb(dungeon_heart->terminated_queue, imp);
				break;
			default:
				mylog(L_ERR, "Imp[%d] returned bad code %d, this must be a BUG!\n", imp->id, ret);
    			queue_enqueue_nb(dungeon_heart->terminated_queue, imp);
				break;
		}
	}
}


int imp_set_ioev(imp_t *imp, int fd, struct epoll_event *ev)
{
	return imp_body_set_ioev(imp->body, fd, ev);
}

int imp_get_ioev(imp_t *imp, struct epoll_event *ev)
{
	return imp_body_get_ioev(imp->body, ev);
}

int imp_set_timer(imp_t *imp, struct itimerval *itv)
{
	return imp_body_set_timer(imp->body, itv);
}

