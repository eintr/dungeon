#include <stdio.h>
#include <stdlib.h>

#include "proxy.h"

proxy_context_t *proxy_context_new(proxy_pool_t *pool)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		newnode->state = STATE_START;
		newnode->listen_sd = dup(pool->original_listen_sd);
		if (newnode->listen_sd<0) {
			mylog();
			goto drop_and_fail;
		}
		newnode->client_conn = NULL;
		newnode->http_header_buffer = malloc(HTTP_HEADER_MAX+1);
		newnode->http_header_buffer_pos = 0;
		newnode->http_header = NULL;
		newnode->server_conn = NULL;
		newnode->s2c_buf = buffer_new(0);
		newnode->c2s_buf = buffer_new(0);
		newnode->errlog_str = "No error.";
	}
	return newnode;
drop_and_fail:
	free(newnode);
	return NULL;
}

int proxy_context_delete(proxy_context_t *my)
{
	if (my->state != STATE_TERM) {
		mylog();
	}
	if (buffer_nbytes(my->client_rbuf)) {
		mylog();
	}
	if (buffer_nbytes(my->client_wbuf)) {
		mylog();
	}
	if (buffer_delete(my->client_rbuf)) {
		mylog();
	}
	free(my->http_header_buffer);
	free(my->http_header);
	if (connection_close(my->client_conn)) {
		mylog();
	}
	if (buffer_nbytes(my->server_rbuf)) {
		mylog();
	}
	if (buffer_nbytes(my->server_wbuf)) {
		mylog();
	}
	if (buffer_delete(my->server_rbuf)) {
		mylog();
	}
	if (connection_close(my->server_conn)) {
		mylog();
	}
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
			ev.events = EPOLL_OUT|EPOLLONESHOT;
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my->listen_sd, &ev);
			break;
		case STATE_READHEADER:
			ev.events = EPOLL_IN|EPOLLONESHOT;
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			break;
		case STATE_CONNECTSERVER:
			ev.events = EPOLL_IN|EPOLL_OUT|EPOLLONESHOT;
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
			break;
		case STATE_IOWAIT:
			ev.events = EPOLLIN|EPOLLONESHOT;
			if (buffer_nbytes(my->c2s_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
			ev.events = EPOLLIN|EPOLLONESHOT;
			if (buffer_nbytes(my->s2c_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			epoll_ctl(epoll_fd, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			break;
		default:
			mylog();
			proxy_context_put_runqueue(my);
			break;
	}
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	my->client_conn = connection_accept(my->listen_sd);
	if (my->client_conn==NULL) {
		if (errno==EAGAIN || errno==EINTR) {
			mylog();
		} else {
			mylog();
			return -1;
		}
	}
	my->state = STATE_READHEADER;
	proxy_context_put_epollfd(my);
	return 0;
}

static int proxy_context_driver_readheader(my->listen_sd)
{
	ssize_t len;

	memset(hdrbuf, 0, HEADERSIZE);
	for (pos=0;pos<HEADERSIZE;) {
		len = connection_read(my->client_conn, my->http_header_buffer+my->http_header_buffer_pos, HEADERSIZE-pos-1);
		if (len<0) {
			mylog();
			my->errlog_str = "Header is too long.";
			my->state = STATE_ERR;
			proxy_context_put_runqueue(my);
			return -1;
		}
		my->http_header_buffer_pos += len;
		if (strstr(hdrbuf, "\r\n\r\n") || strstr(hdrbuf, "\n\n")) {
			mylog();
			if (buffer_write(my->c2s_buf, hdrbuf, pos)<0) {
				mylog();
				my->errlog_str = "The first write of c2s_buf failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			if (my->http_header = http_header_parse(hdrbuf)==NULL) {
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			my->state = STATE_CONNECTSERVER;
			proxy_context_put_epollfd(my);
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

	err = connection_connect_nb(&my->server_conn);
	if (err==0 || err==EISCONN) {
		my->state = STATE_IOWAIT;
		proxy_context_put_epollfd(my);
	} else if (err==AA_ETIMEOUT) {
		// Register this fail and
		my->state = STATE_REJECTCLIENT;
		proxy_context_put_runqueue(my);
	}
	proxy_context_put_epollfd(my);
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
		case STATE_RELAY:
			ret = proxy_context_driver_relay(my);
			break;
		case STATE_IOWAIT:
			ret = proxy_context_driver_iowait(my);
			break;
		case STATE_IO:
			ret = proxy_context_driver_io(my);
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

