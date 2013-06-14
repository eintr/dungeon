#ifndef PROXY_CONTEXT_H
#define PROXY_CONTEXT_H


#include "aa_connection.h"
#include "aa_http.h"
#include "aa_bufferlist.h"
#include "proxy_pool.h"

enum proxy_state_en {
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
	STATE_ERR,
	STATE_TERM,
	STATE_CLOSE
};

#define	HTTP_HEADER_MAX	4096
#define HEADERSIZE HTTP_HEADER_MAX

#define DATA_SENDSIZE 4096

typedef struct proxy_context_st {
	proxy_pool_t *pool;
	int state;
	int listen_sd;

	int epoll_context;

	connection_t *client_conn;
	struct timeval client_r_timeout_tv, client_s_timeout_tv;
	char *http_header_buffer;
	int http_header_buffer_pos;
	struct http_header_st http_header;

	connection_t *server_conn;
	char *server_ip;
	int server_port;

	struct timeval server_r_timeout_tv, server_s_timeout_tv;

	buffer_list_t *s2c_buf, *c2s_buf;
	int s2c_wactive, c2s_wactive;
	
	char *errlog_str;
} proxy_context_t;

proxy_context_t *proxy_context_new_accepter(proxy_pool_t *pool);

int proxy_context_delete(proxy_context_t*);

int proxy_context_timedout(proxy_context_t*);

int is_proxy_context_timedout(proxy_context_t*);

int proxy_context_put_runqueue(proxy_context_t*);

int proxy_context_put_epollfd(proxy_context_t*);

int proxy_context_driver(proxy_context_t*);

#endif

