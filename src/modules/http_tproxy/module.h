#ifndef HTTP_TPROXY_CONTEXT_H
#define HTTP_TPROXY_CONTEXT_H

#include <stdint.h>

#include "cJSON.h"

#include "aa_connection.h"
#include "aa_http.h"
#include "aa_bufferlist.h"
#include "proxy_pool.h"
#include "aa_state_dict.h"

enum http_tproxy_state_en {
	STATE_ACCEPT_CREATE = 1,
	STATE_ACCEPT,
	STATE_READHEADER,
	STATE_PARSEHEADER,
	STATE_DNS,
	STATE_REGISTERSERVERFAIL,
	STATE_REJECTCLIENT,
	STATE_CONNECTSERVER,
	STATE_RELAY,
	STATE_IOWAIT,
	STATE_DNS_PROBE,                     
	STATE_CONN_PROBE, 
	STATE_ERR,
	STATE_TERM,
	STATE_CLOSE
};

#define	HTTP_HEADER_MAX	4096
#define HEADERSIZE HTTP_HEADER_MAX

#define DATA_SENDSIZE 4096

typedef struct http_tproxy_context_st {
	enum http_tproxy_state_en state;
	int header_sent;

	int listen_sd;

	int epoll_context;
	int timer;

	connection_t *client_conn;
	struct timeval client_r_timeout_tv, client_s_timeout_tv;
	char *http_header_buffer;
	int http_header_buffer_pos;
	struct http_header_st http_header;

	connection_t *server_conn;
	char *server_ip;
	int server_port;

	struct timeval server_r_timeout_tv, server_s_timeout_tv;
	struct timeval server_connect_timeout_tv;

	buffer_list_t *s2c_buf, *c2s_buf;
	int s2c_buffull, c2s_buffull;
	
	char *errlog_str;
	int set_dict;
} http_tproxy_context_t;

http_tproxy_context_t *http_tproxy_context_new_accepter(proxy_pool_t *pool);

int http_tproxy_context_delete(http_tproxy_context_t*);

int http_tproxy_context_timedout(http_tproxy_context_t*);

int is_proxy_context_timedout(http_tproxy_context_t*);

int http_tproxy_context_put_runqueue(http_tproxy_context_t*);

int http_tproxy_context_put_epollfd(http_tproxy_context_t*);

int http_tproxy_context_driver(http_tproxy_context_t*);

cJSON *http_tproxy_context_serialize(http_tproxy_context_t*);

#endif

