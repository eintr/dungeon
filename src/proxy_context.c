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
	if (aa_connection_close(my->client_conn)) {
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
	if (aa_connection_close(my->server_conn)) {
		mylog();
	}
	free(my);
	return 0;
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	// Put my->client_conn into epoll_fd.
	my->client_conn = aa_connection_accept(my->listen_sd);
	if (my->client_conn==NULL) {
		if (errno==EAGAIN || errno==EINTR) {
			mylog();
		} else {
			mylog();
			return -1;
		}
	}
	my->state = STATE_READHEADER;
	return 0;
}

static int proxy_context_driver_readheader(my->listen_sd)
{
	ssize_t len;

	memset(hdrbuf, 0, HEADERSIZE);
	for (pos=0;pos<HEADERSIZE;) {
		len = aa_connection_read(my->client_conn, my->http_header_buffer+my->http_header_buffer_pos, HEADERSIZE-pos-1);
		if (len<0) {
			mylog();
			my->errlog_str = "Header is too long.";
			my->state = STATE_ERR;
			return -1;
		}
		my->http_header_buffer_pos += len;
		if (strstr(hdrbuf, "\r\n\r\n") || strstr(hdrbuf, "\n\n")) {
			mylog();
			if (buffer_write(my->server_wbuf, hdrbuf, pos)<0) {
				mylog();
				my->errlog_str = "The first write of server_wbuf failed.";
				my->state = STATE_ERR;
				return -1;
			}
			if (my->http_header = http_header_parse(hdrbuf)==NULL) {
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				return -1;
			}
			my->state = STATE_CONNECTSERVER;
			return 0;
		}
	}
}

static int proxy_context_driver_connectserver(proxy_context_t *my)
{
	int err;

	err = aa_connection_connect_nb(&my->server_conn);
	if (err==0 || err==EISCONN) {
		my->state = STATE_IOWAIT;
	} else if (err==AA_ETIMEOUT) {
		// Register this fail and
		my->state = STATE_REJECTCLIENT;
	}
	return 0;
}

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
	if (!iowait_needed(my->state)) {
		// Put my into run queue.
	} else {
		// Put my events to epoll_fd.
	}
	return ret;
}

