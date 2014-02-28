#include <stdio.h>

#include "imp.h"
#include "dungeon.h"

uint32_t global_context_id___=1;

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

