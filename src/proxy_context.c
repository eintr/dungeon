#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>

#include "proxy_context.h"
#include "aa_err.h"
#include "aa_log.h"

proxy_context_t *proxy_context_new_accepter(proxy_pool_t *pool)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		newnode->state = STATE_ACCEPT;
		newnode->listen_sd = dup(pool->original_listen_sd);
		if (newnode->listen_sd<0) {
			mylog(L_ERR, "dup listen_sd failed");
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

static proxy_context_t *proxy_context_new(proxy_pool_t *pool, connection_t *client_conn)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		newnode->state = STATE_READHEADER;
		newnode->listen_sd = -1;
		newnode->client_conn = client_conn;
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
		mylog(L_WARNING, "improper state, proxy is running");
	}
	if (buffer_nbytes(my->c2s_buf)) {
		mylog(L_WARNING, "some data is in c2s buffer, poping");
		while (buffer_pop(my->c2s_buf) == 0);
	}
	if (buffer_nbytes(my->s2c_buf)) {
		mylog(L_WARNING, "some data is in s2c buffer, poping");
		while (buffer_pop(my->s2c_buf) == 0);
	}
	if (buffer_delete(my->c2s_buf)) {
		mylog(L_ERR, "delete c2s buffer failed");
	}
	if (buffer_delete(my->s2c_buf)) {
		mylog(L_ERR, "delete s2c buffer failed");
	}

	free(my->http_header_buffer);
	
	if (connection_close_nb(my->client_conn)) {
		mylog(L_ERR, "close client connectin failed");
	}
	if (connection_close_nb(my->server_conn)) {
		mylog(L_ERR, "close server connectin failed");
	}

	close(my->listen_sd);
	free(my);
	return 0;
}

int proxy_context_put_runqueue(proxy_context_t *my)
{
	if (llist_append_nb(my->pool->run_queue, my)) {
		mylog(L_ERR, "put context to run queue failed");
	}
	mylog(L_DEBUG, "put context to run queue success");
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
			ev.events = EPOLLIN | EPOLLONESHOT;
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			break;
		case STATE_CONNECTSERVER:
			ev.events = EPOLLOUT | EPOLLONESHOT;
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
			mylog(L_ERR, "unknown state");
			//abort();
//			proxy_context_put_runqueue(my);
			break;
	}
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	proxy_context_t *newproxy;
	connection_t *client_conn;
	int ret;

	ret = connection_accept_nb(&client_conn, my->listen_sd);
	if (ret) {
		if (ret==EAGAIN || ret==EINTR) {
			mylog(L_DEBUG, "nonblock accept, wait for next turn");
			proxy_context_put_epollfd(my);
			return -1;
		} else {
			mylog(L_ERR, "accept failed");
			return -1;
		}
	}
	// TODO: Check pool configure to refuse redundent connections.
	if (my->pool->nr_idle + my->pool->nr_busy +1 > my->pool->nr_total) {
		newproxy = proxy_context_new(my->pool, client_conn);
		proxy_context_put_runqueue(newproxy);
	} else {
		mylog(L_ERR, "refuse connection");
		connection_close_nb(client_conn);
	}
	return 0;
}

static int proxy_context_driver_readheader(proxy_context_t *my)
{
	ssize_t len;
	char *server_ip, *tmp;
	uint8_t server_port;
	char *hdrbuf = my->http_header_buffer;
	int pos;

	memset(my->http_header_buffer, 0, HEADERSIZE);
	for (pos = 0; pos<HEADERSIZE; ) {
		len = connection_recv_nb(my->client_conn, 
				my->http_header_buffer + my->http_header_buffer_pos, HEADERSIZE - pos - 1);
		if (len<0) {
			mylog(L_ERR, "read header failed");
			my->errlog_str = "Header is too long.";
			my->state = STATE_ERR;
			proxy_context_put_runqueue(my);
			return -1;
		}
		my->http_header_buffer_pos += len;
		//TODO: write body data to c2s_buf
		if (strstr(hdrbuf, "\r\n\r\n") || strstr(hdrbuf, "\n\n")) {
			mylog(L_DEBUG, "deal header data");
			if (buffer_write(my->c2s_buf, hdrbuf, pos)<0) {
				mylog(L_ERR, "write header to c2s buffer failed");
				my->errlog_str = "The first write of c2s_buf failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			if (http_header_parse(&my->http_header, hdrbuf) < 0) {
				mylog(L_ERR, "parse header failed");
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}

			server_ip = malloc(my->http_header.host.size + 1);
			if (server_ip == NULL) {
				mylog(L_ERR, "no memory for header host");
				my->errlog_str = "Http header read server ip failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			memcpy(server_ip, my->http_header.host.ptr, my->http_header.host.size);
			server_ip[my->http_header.host.size] = '\0';
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

	if (err==0 || err==EISCONN) {
		my->state = STATE_IOWAIT;
		proxy_context_put_epollfd(my);
	} else if (err==AA_ETIMEOUT) {
		my->state = STATE_REJECTCLIENT;
		proxy_context_put_runqueue(my);
	}
	proxy_context_put_epollfd(my); //EINPROGRESS

	return 0;
}

/*
 * Tell server_state_dict the server was timed out.
 * TODO: distinguish between slow and down.
 */
static int proxy_context_driver_registerserverfail(proxy_context_t *my)
{
	//server_state_set();
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

int proxy_context_connection_failed(proxy_context_t *my)
{
	//TODO: generate error message to client
	return 0;
}

/*
 * Receive & send data between client and server
 */
int proxy_context_driver_iowait(proxy_context_t *my)
{
	connection_t *client_conn, server_conn;
	int res;

	client_conn = my->client_conn;

	/* recv from server */
	while (my->s2c_wactive) {
		res = connection_recvv_nb(my->server_conn, my->s2c_buf, DATA_BUFMAX);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				mylog(L_WARNING, "nonblock receive data from server failed, try again");
				break; //non-block
			} else {
				/* generate error message to client */
				mylog(L_ERR, "receive data from server failed, close connection");
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
				mylog(L_WARNING, "nonblock send data to client failed, try again");
				break; //non-block
			} else {
				// client error
				mylog(L_ERR, "send data from client failed, close connection");
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
		res = connection_recvv_nb(my->client_conn, my->c2s_buf, DATA_BUFSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				mylog(L_WARNING, "nonblock receive data from client failed, try again");
				break; //non-block
			} else {
				mylog(L_ERR, "receive data from client failed, close connection");
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
				mylog(L_WARNING, "nonblock send data to server failed, try again");
				break; //non-block
			} else {
				/* generate error message to client */
				mylog(L_ERR, "send data to server failed, close connection");
				proxy_context_connection_failed(my);
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
			mylog(L_ERR, "context driver error occurred");
			break;
		case STATE_TERM:
			break;
		default:
			break;
	}
	return ret;
}

