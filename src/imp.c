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

void imp_set_run(struct dungeon_st *pool, imp_t *cont)
{
    llist_append(pool->run_queue, cont);
}

void imp_set_iowait(struct dungeon_st *pool, int fd, imp_t *cont)
{
    struct epoll_event ev;

    ev.events = EPOLLIN|EPOLLOUT|EPOLLONESHOT;
    ev.data.ptr = cont;
    epoll_ctl(pool->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void imp_set_term(struct dungeon_st *pool, imp_t *cont)
{
    llist_append(pool->terminated_queue, cont);
}

