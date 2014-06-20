/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
/** \endcond */

/** \file mod.c
	This is a test module, it reflects every byte of the socket.
*/

#include <room_service.h>

#define	BUFSIZE	4096

struct push_service_memory_st {
	conn_tcp_t	*conn;
	int state;
	uint8_t buf[BUFSIZE];
	off_t len, pos;
};

static imp_soul_t push_service_soul;

enum state_en {
	ST_RECV=1,
	ST_SEND,
	ST_TERM,
	ST_Ex
};

static void *push_service_new(imp_t *imp)
{
	struct push_service_memory_st *m=imp->memory;

	m->state = ST_RECV;
	m->len = 0;
	m->pos = 0;
	return NULL;
}

static int push_service_delete(imp_t *imp)
{
	struct push_service_memory_st *memory = imp->memory;

	conn_tcp_close_nb(memory->conn);
	free(memory);
	imp->memory = NULL;
	return 0;
}

static enum enum_driver_retcode push_service_driver(imp_t *imp)
{
	struct push_service_memory_st *mem;
	struct epoll_event ev;
	ssize_t ret;

	mem = imp->memory;
	switch (mem->state) {
		case ST_RECV:
			if (imp->event_mask & EV_MASK_TIMEOUT) {
				mylog(L_INFO, "[%d]: recv() timed out.", imp->id);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			mem->len = conn_tcp_recv_nb(mem->conn, mem->buf, BUFSIZE);
			if (mem->len == 0) {
				mem->state = ST_TERM;
				return TO_RUN;
			} else if (mem->len < 0) {
				if (errno==EAGAIN) {
					ev.events = EPOLLIN|EPOLLRDHUP;
					ev.data.ptr = imp;
					if (imp_set_ioev(imp, mem->conn->sd, &ev)<0) {
						mylog(L_ERR, "imp[%d]: Failed to imp_set_ioev() %m, suicide.");
						return TO_TERM;
					}
					return TO_WAIT_IO;
				} else {
					mem->state = ST_Ex;
					return TO_RUN;
				}
			} else {
				mem->pos = 0;
				mem->state = ST_SEND;
				return TO_RUN;
			}
			break;
		case ST_SEND:
			if (imp->event_mask & EV_MASK_TIMEOUT) {
				mylog(L_INFO, "[%d]: send(): timed out.", imp->id);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			ret = conn_tcp_send_nb(mem->conn, mem->buf + mem->pos, mem->len);
			if (ret<=0) {
				if (errno==EAGAIN) {
					ev.events = EPOLLOUT|EPOLLRDHUP;
					ev.data.ptr = imp;
					if (imp_set_ioev(imp, mem->conn->sd, &ev)<0) {
						mylog(L_ERR, "imp[%d]: Failed to imp_set_ioev() %m, suicide.");
						return TO_TERM;
					}
					return TO_WAIT_IO;
				} else {
					mem->state = ST_Ex;
					return TO_RUN;
				}
			} else {
				mem->pos += ret;
				mem->len -= ret;
				if (mem->len <= 0) {
					mem->state = ST_RECV;
					return TO_RUN;
				} else {
					ev.events = EPOLLOUT|EPOLLRDHUP;
					ev.data.ptr = imp;
					if (imp_set_ioev(imp, mem->conn->sd, &ev)<0) {
						mylog(L_ERR, "imp[%d]: Failed to imp_set_ioev() %m, suicide.");
						return TO_TERM;
					}
					return TO_WAIT_IO;
				}
			}
			break;
		case ST_Ex:
			mylog(L_INFO, "[%d]: Exception happened.", imp->id);
			mem->state = ST_TERM;
			break;
		case ST_TERM:
			return TO_TERM;
			break;
		default:
			break;
	}
	return TO_RUN;
}

static void *push_service_serialize(imp_t *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

imp_t *push_service_summon(conn_tcp_t *conn)
{
	imp_t *server;
	struct push_service_memory_st *mem;

	mem = calloc(sizeof(*mem), 1);
	mem->conn = conn;
	server = imp_summon(mem, &push_service_soul);
	if (server==NULL) {
		mylog(L_ERR, "Failed to summon a new push_service.");
		free(mem);
		return NULL;
	}
	return server;
}

static imp_soul_t push_service_soul = {
	.fsm_new = push_service_new,
	.fsm_delete = push_service_delete,
	.fsm_driver = push_service_driver,
	.fsm_serialize = push_service_serialize,
};

