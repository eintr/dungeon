#include <stdio.h>
#include <stdlib.h>

#include "imp.h"
#include "dungeon.h"
#include "util_log.h"

/** Global imp id serial generator */
uint32_t global_imp_id___=1;
#define GET_NEXT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 1)

imp_t *imp_new(imp_soul_t *soul)
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

int imp_delete(imp_t *imp)
{
	imp->soul->fsm_delete(imp->memory);
	mylog(L_DEBUG, "Deleting imp[%d]->body.", imp->id);
	imp_body_delete(imp->body);
	mylog(L_DEBUG, "Deleting imp[%d].", imp->id);
	free(imp);
	return 0;
}

void imp_set_run(imp_t *cont)
{
    llist_append(dungeon_heart->run_queue, cont);
}

void imp_set_iowait(int fd, imp_t *imp)
{
    struct epoll_event ev;

    ev.events = EPOLLIN|EPOLLONESHOT;
    ev.data.ptr = imp;
    epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void imp_set_term(imp_t *cont)
{
    llist_append(dungeon_heart->terminated_queue, cont);
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

