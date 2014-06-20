/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
/** \endcond */

/** \file mod.c
	This is a test module, it reflects every byte of the socket.
*/

#include <room_service.h>

#include "push_api.h"

struct listener_memory_st {
	listen_tcp_t	*listen;
};

static imp_soul_t listener_soul;
static struct addrinfo *local_addr;
static timeout_msec_t timeo;

static imp_t *id_listener;

static void msec_2_timeval(struct timeval *tv, uint32_t ms)
{
    tv->tv_sec = ms/1000;
    tv->tv_usec = (ms%1000)*1000;
}

static int get_config(cJSON *conf)
{
	int ga_err;
	struct addrinfo hint;
	cJSON *value;
	char *Host, Port[12];

	hint.ai_flags = 0;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	value = cJSON_GetObjectItem(conf, "API_LocalAddr");
	if (value->type != cJSON_String) {
		mylog(L_INFO, "Module is configured with illegal LocalAddr.");
		return -1;
	} else {
		Host = value->valuestring;
	}

	value = cJSON_GetObjectItem(conf, "API_LocalPort");
	if (value->type != cJSON_Number) {
		mylog(L_INFO, "Module is configured with illegal LocalPort.");
		return -1;
	} else {
		sprintf(Port, "%d", value->valueint);
	}

	ga_err = getaddrinfo(Host, Port, &hint, &local_addr);
	if (ga_err) {
		mylog(L_INFO, "Module configured with illegal LocalAddr and LocalPort.");
		return -1;
	}

	value = cJSON_GetObjectItem(conf, "API_TimeOut_Recv_ms");
	if (value->type != cJSON_Number) {
		mylog(L_INFO, "Module is configured with illegal TimeOut_Recv_ms.");
		return -1;
	} else {
		timeo.recv = value->valueint;
	}

	value = cJSON_GetObjectItem(conf, "API_TimeOut_Send_ms");
	if (value->type != cJSON_Number) {
		mylog(L_INFO, "Module is configured with illegal TimeOut_Send_ms.");
		return -1;
	} else {
		timeo.send = value->valueint;
	}

	return 0;
}

static int mod_init(cJSON *conf)
{
	struct listener_memory_st *l_mem;

	if (get_config(conf)!=0) {
		return -1;
	}

	l_mem = malloc(sizeof(*l_mem));
	l_mem->listen = conn_tcp_listen_create(local_addr, &timeo);

	id_listener = imp_summon(l_mem, &listener_soul);
	if (id_listener==NULL) {
		mylog(L_ERR, "imp_summon() Failed!\n");
		return 0;
	}
	return 0;
}

static int mod_destroy(void)
{
	imp_kill(id_listener);
	return 0;
}

static void mod_maint(void)
{
}

static cJSON *mod_serialize(void)
{
	return NULL;
}

static void *listener_new(imp_t *p)
{
	struct listener_memory_st *lmem=p->memory;
	struct epoll_event ev;

	ev.events = EPOLLIN|EPOLLRDHUP;
	ev.data.fd = lmem->listen->sd;
	if (imp_set_ioev(p, lmem->listen->sd, &ev)<0) {
		mylog(L_ERR, "Set listen socket epoll event FAILED: %m\n");
	}

	return NULL;
}

static int listener_delete(imp_t *p)
{
	struct listener_memory_st *mem=p->memory;

	free(mem);
	return 0;
}

static enum enum_driver_retcode listener_driver(imp_t *p)
{
	struct listener_memory_st *lmem=p->memory;
	struct server_memory_st *emem;
	conn_tcp_t *conn;
	imp_t *server;
	struct epoll_event ev;

	// TODO: Disable timer here.

	while (conn_tcp_accept_nb(&conn, lmem->listen, &timeo)==0) {
		server = push_api_summon(conn);
		if (server==NULL) {
			mylog(L_ERR, "Failed to summon a new api imp.");
			conn_tcp_close_nb(conn);
			continue;
		}
	}

	return TO_WAIT_IO;
}

static void *listener_serialize(imp_t *imp)
{
	struct listener_memory_st *m=imp->memory;

	return NULL;
}

static imp_soul_t listener_soul = {
	.fsm_new = listener_new,
	.fsm_delete = listener_delete,
	.fsm_driver = listener_driver,
	.fsm_serialize = listener_serialize,
};

module_interface_t api_interface = 
{
	.mod_initializer = mod_init,
	.mod_destroier = mod_destroy,
	.mod_maintainer = mod_maint,
	.mod_serialize = mod_serialize,
};

