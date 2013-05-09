#include <stdio.h>
#include <stdlib.h>

#include "proxy_context.h"

proxy_context_t *proxy_context_new_accepter(proxy_pool_t *pool)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		newnode->state = STATE_ACCEPT;
		newnode->listen_sd = dup(pool->original_listen_sd);
		if (newnode->listen_sd<0) {
			mylog();
			goto drop_and_fail;
		}
		newnode->client_conn = NULL;
		newnode->http_header_buffer = NULL;
		newnode->http_header_buffer_pos = 0;
		//newnode->http_header = NULL;
		newnode->server_conn = NULL;
		newnode->s2c_buf = NULL;
		newnode->c2s_buf = NULL;
		newnode->s2c_wactive = 1;
		newnode->c2s_wactive = 1;
		newnode->errlog_str = "No error.";
	}
	return newnode;
drop_and_fail:
	free(newnode);
	return NULL;
}

static proxy_context_t *proxy_context_new(proxy_pool_t *pool, proxy_context_t *parent)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		newnode->state = STATE_READHEADER;
		newnode->listen_sd = -1;
		newnode->client_conn = parent->client_conn;
		newnode->http_header_buffer = malloc(HTTP_HEADER_MAX+1);
		newnode->http_header_buffer_pos = 0;
		//newnode->http_header = NULL;
		newnode->server_conn = NULL;
		newnode->s2c_buf = buffer_new(0);
		newnode->c2s_buf = buffer_new(0);
		newnode->s2c_wactive = 1;
		newnode->c2s_wactive = 1;
		newnode->data_buf = (char *) malloc(DATA_BUFSIZE);
		newnode->errlog_str = "No error.";
	}
	return newnode;
drop_and_fail:
	free(newnode);
	return NULL;
}

int proxy_context_delete(proxy_context_t *my)
{
	if (my->state != STATE_TERM || my->state != STATE_CLOSE || my->state != STATE_ERR) {
		mylog();
	}
	if (buffer_nbytes(my->c2s_buf)) {
		mylog();
		while (buffer_pop(my->c2s_buf) == 0);
	}
	if (buffer_nbytes(my->s2c_buf)) {
		mylog();
		while (buffer_pop(my->s2c_buf) == 0);
	}
	if (buffer_delete(my->c2s_buf)) {
		mylog();
	}
	if (buffer_delete(my->s2c_buf)) {
		mylog();
	}

	free(my->http_header_buffer);
	
	if (connection_close_nb(my->client_conn)) {
		mylog();
	}
	if (connection_close_nb(my->server_conn)) {
		mylog();
	}

	close(my->listen_sd);
	free(my);
	return 0;
}

int proxy_context_put_runqueue(proxy_context_t *my)
{
	if (llist_append_nb(my->pool->run_queue, my)) {
		mylog();
	}
	mylog();
	return 0;
}

int proxy_context_put_epollfd(proxy_context_t *my)
{
	struct epoll_event ev;

	ev.data.ptr = my;
	switch (my->state) {
		case STATE_ACCEPT:
			ev.events = EPOLLOUT;
			epoll_ctl(my->pool->epoll_accept, EPOLL_CTL_MOD, my->listen_sd, &ev);
			break;
		case STATE_READHEADER:
			ev.events = EPOLL_IN|EPOLLONESHOT;
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			break;
		case STATE_CONNECTSERVER:
			ev.events = EPOLL_OUT|EPOLLONESHOT;
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
			break;
		case STATE_IOWAIT:
			ev.events = EPOLLONESHOT;
			/* If buffer is not full*/
			if (my->c2s_wactive) {
				ev.events |= EPOLLIN;
			}
			if (buffer_nbytes(my->c2s_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->server_conn->sd, &ev);

			ev.events = EPOLLONESHOT;
			/* If buffer is not full*/
			if (my->s2c_wactive) {
				ev.events |= EPOLLIN;
			}
			if (buffer_nbytes(my->s2c_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			break;
		default:
			mylog();
			abort();
//			proxy_context_put_runqueue(my);
			break;
	}
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	proxy_context_t *newproxy;
	connection_t *client_conn;

	client_conn = connection_accept(my->listen_sd);
	if (client_conn==NULL) {
		if (errno==EAGAIN || errno==EINTR) {
			mylog();
			proxy_context_put_epollfd(my);
			return -1;
		} else {
			mylog();
			return -1;
		}
	}
	// TODO: Check pool configure to refuse redundent connections.
	if (my->pool->nr_idle + my->pool->nr_busy +1 > my->pool->nr_total) {
		newproxy = proxy_context_new(my->pool, client_conn);
		proxy_context_put_runqueue(newproxy);
	} else {
		mylog();
		connection_delete(client_conn);
	}
	return 0;
}

static int proxy_context_driver_readheader(proxy_context_t *my)
{
	ssize_t len;
	char *server_ip, *tmp;
	uint8_t server_port;
	char *hdrbuf = my->http_header_buffer;

	memset(my->http_header_buffer, 0, HEADERSIZE);
	for (pos=0;pos<HEADERSIZE;) {
		len = connection_recv_nb(my->client_conn, 
				my->http_header_buffer + my->http_header_buffer_pos, HEADERSIZE - pos - 1);
		if (len<0) {
			mylog();
			my->errlog_str = "Header is too long.";
			my->state = STATE_ERR;
			proxy_context_put_runqueue(my);
			return -1;
		}
		my->http_header_buffer_pos += len;
		//TODO: write body data to c2s_buf
		if (strstr(hdrbuf, "\r\n\r\n") || strstr(hdrbuf, "\n\n")) {
			mylog();
			if (buffer_write(my->c2s_buf, hdrbuf, pos)<0) {
				mylog();
				my->errlog_str = "The first write of c2s_buf failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			if (http_header_parse(&my->http_header, hdrbuf) < 0) {
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}

			server_ip = malloc(my->http_header->host->size + 1);
			if (server_ip == NULL) {
				my->errlog_str = "Http header read server ip failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			memcpy(server_ip, my->http_header->host->ptr, my->http_header->host->size);
			server_ip[my->http_header->host->size] = '\0';
			tmp = strchr(server_ip, ':');
			if (tmp) {
				tmp = '\0';
				server_port = atoi(tmp + 1);
			} else {
				server_port = 80;
			}
			my->server_ip = server_ip;
			my->server_port = server_port;

			my->state = STATE_CONNECTSERVER;
			proxy_context_put_runqueue(my);
			return 0;
		}
	}
}

/*
 * Try to build the server size connection.
 * After this state, both connection are r/w ready.
 */
static int proxy_context_driver_connectserver(proxy_context_t *my)
{
	int err;

	err = connection_connect_nb(&my->server_conn, my->server_ip, my->server_port);

	if (err == 0) {
		my->state = STATE_IOWAIT;
		proxy_context_put_epollfd(my);
	} else if (err == 1) {
		proxy_context_put_epollfd(my); //EINPROGRESS
	} else if (err == -1) {
		my->state = STATE_REJECTCLIENT;
		proxy_context_put_runqueue(my);
	} else if (err == -2) {
		my->state = STATE_REGEISTERSERVERFAIL;
		proxy_context_put_runqueue(my);
	}

	return 0;
}

/*
 * Tell server_state_dict the server was timed out.
 * TODO: distinguish between slow and down.
 */
static int proxy_context_driver_registerserverfail(proxy_context_t *my)
{
	server_state_set();
}

/*
 * Process proxy context timeout.
 */
int proxy_context_timedout(proxy_context_t *my)
{
	my->state = STATE_REGISTERSERVERFAIL;
	proxy_context_put_runqueue(my);
}

/*
 * Test if the server side was timed out.
 * Client timeout not supported yet.
 */
int is_proxy_context_timedout(proxy_context_t *my)
{
	struct timeval now, past, diff;

	gettimeofday(&now, NULL);
	if (timerisset(&my->server_r_timeout_tv)) {
		timersub(&now, &my->server_conn->last_r_tv, &past);
		timersub(&past, &my->server_r_timeout_tv, &diff);
		if (diff.tv_sec>0) {
			return 1;
		}
	}
	if (timerisset(&my->server_s_timeout_tv)) {
		timersub(&now, &my->server_conn->last_s_tv, &past);
		timersub(&past, &my->server_s_timeout_tv, &diff);
		if (diff.tv_sec>0) {
			return 1;
		}
	}
	return 0;
}
int proxy_context_driver_iowait(proxy_context_t *my)
{
	connection_t *client_conn, server_conn;
	int res;

	client_conn = my->client_conn;

	/* recv from server */
	while (my->s2c_wactive) {
		res = connection_recvv_nb(my->server_conn, my->s2c_buf, DATA_BUFSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				break; //non-block
			} else {
				/* generate error message to client */
				proxy_context_connection_failed(my);
				my->state = STATE_CLOSE;
				return 0;
			}
		}

		if (my->s2c_buf->bufsize == my->s2c_buf->max) {
			my->s2c_wactive = 0;
			break;
		}
	}
	
	/* send to client */
	while (buffer_nbytes(my->s2c_buf) > 0) {
		res = connection_sendv_nb(my->client_conn, my->s2c_buf, DATA_SENDSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				break; //non-block
			} else {
				// client error
				my->state = STATE_CLOSE;
				return 0;
			}
		}
		if (my->s2c_buf->bufsize < my->s2c_buf->max) {
			my->s2c_wactive = 1;
		}

	}

	/* recv from client */
	while (my->c2s_wactive) {
		res = connection_recvv_nb(my->server_conn, my->c2s_buf, DATA_BUFSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				break; //non-block
			} else {
				//client error
				my->state = STATE_CLOSE;
				return 0;
			}
		}
		if (my->c2s_buf->bufsize == my->c2s_buf->max) {
			my->c2s_wactive = 0;
			break;
		}
	}

	/* send to server */
	while (buffer_nbytes(my->c2s_buf) > 0) {
		res = connection_sendv_nb(my->server_conn, my->c2s_buf, DATA_SENDSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				break; //non-block
			} else {
				/* generate error message to client */
				proxy_context_server_failed(my);
				my->state = STATE_CLOSE;
				return 0;
			}
		}
		if (my->c2s_buf->bufsize < my->c2s_buf->max) {
			my->c2s_wactive = 1;
		}
	}

	return 0;
}

int proxy_context_driver_close(proxy_context_t *my)
{
	return proxy_context_delete(my);
}

/*
 * Main driver of proxy context FSA
 */
int proxy_context_driver(proxy_context_t *my)
{
	int ret;
	switch (my->state) {
		case STATE_ACCEPT:
			ret = proxy_context_driver_accept(my);
			break;
		case STATE_READHEADER:
			ret = proxy_context_driver_readheader(my);
			break;
		case STATE_CONNECTSERVER:
			ret = proxy_context_driver_connectserver(my);
			break;
		case STATE_IOWAIT:
			ret = proxy_context_driver_iowait(my);
			break;
		case STATE_CLOSE:
			ret = proxy_context_driver_close(my);
			break;
		case STATE_ERR:
			mylog();
			break;
		case STATE_TERM:
			break;
		default:
			break;
	}
	return ret;
}

