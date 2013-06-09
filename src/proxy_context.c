#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "proxy_context.h"
#include "aa_err.h"
#include "aa_log.h"

proxy_context_t *proxy_context_new_accepter(proxy_pool_t *pool)
{
	proxy_context_t *newnode;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		memset(newnode, 0, sizeof(proxy_context_t));
		newnode->pool = pool;
		newnode->state = STATE_ACCEPT_CREATE;
		newnode->listen_sd = dup(pool->original_listen_sd);
		if (newnode->listen_sd<0) {
			//mylog(L_ERR, "dup listen_sd failed");
			goto drop_and_fail;
		}
		newnode->client_conn = NULL;
		newnode->http_header_buffer = NULL;
		newnode->http_header_buffer_pos = 0;
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
		memset(newnode, 0, sizeof(proxy_context_t));
		newnode->pool = pool;
		newnode->state = STATE_READHEADER;
		newnode->listen_sd = -1;
		newnode->client_conn = client_conn;
		newnode->http_header_buffer = malloc(HTTP_HEADER_MAX+1);
		if (newnode->http_header_buffer == NULL) {
			goto drop_and_fail;
		}
		newnode->http_header_buffer_pos = 0;
		newnode->server_conn = NULL;
		newnode->s2c_buf = buffer_new(0);
		if (newnode->s2c_buf == NULL) {
			goto header_buffer_fail;
		}
		newnode->c2s_buf = buffer_new(0);
		if (newnode->c2s_buf == NULL) {
			goto s2c_buf_fail;
		}
		newnode->s2c_wactive = 1;
		newnode->c2s_wactive = 1;
		newnode->errlog_str = "No error.";
	}
	return newnode;

c2s_buf_fail:
	buffer_delete(newnode->c2s_buf);
s2c_buf_fail:
	buffer_delete(newnode->s2c_buf);
header_buffer_fail:
	free(newnode->http_header_buffer);
drop_and_fail:
	free(newnode);
	return NULL;
}

int proxy_context_delete(proxy_context_t *my)
{
	//mylog(L_DEBUG, "In proxy_context_delete");
	fprintf(stderr, "[DEBUG] in proxy_context_delete\n");
	if (my == NULL) {
		return -1;
	}
	if (my->state != STATE_TERM && my->state != STATE_CLOSE && my->state != STATE_ERR) {
		//mylog(L_WARNING, "improper state, proxy is running");
		fprintf(stderr, "improper state, proxy is running\n");
	}
	if (buffer_nbytes(my->c2s_buf)) {
		//mylog(L_WARNING, "some data is in c2s buffer, poping");
		fprintf(stderr, "some data is in c2s buffer, poping\n");
		while (buffer_pop(my->c2s_buf) == 0);
	}
	if (buffer_nbytes(my->s2c_buf)) {
		//mylog(L_WARNING, "some data is in s2c buffer, poping");
		fprintf(stderr, "some data is in s2c buffer, poping\n");
		while (buffer_pop(my->s2c_buf) == 0);
	}
	if (buffer_delete(my->c2s_buf)) {
		//mylog(L_ERR, "delete c2s buffer failed");
		fprintf(stderr, "delete c2s buffer failed\n");
	}
	my->c2s_buf = NULL;
	if (buffer_delete(my->s2c_buf)) {
		//mylog(L_ERR, "delete s2c buffer failed");
		fprintf(stderr, "delete s2c buffer failed\n");
	}
	my->s2c_buf = NULL;

	free(my->http_header_buffer);

	if (my->client_conn && connection_close_nb(my->client_conn)) {
		//mylog(L_ERR, "close client connectin failed");
		fprintf(stderr, "close client connectin failed\n");
	}
	my->client_conn = NULL;
	if (my->server_conn && connection_close_nb(my->server_conn)) {
		//mylog(L_ERR, "close server connectin failed");
		fprintf(stderr, "close server connectin failed\n");
	}
	my->server_conn = NULL;

	close(my->listen_sd);
	//mylog(L_ERR, "{CAUTION} in context close my is %p", my);
	fprintf(stderr, "%u : {CAUTION} in context close my is %p\n", gettid(), my);

	free(my);

	return 0;
}

int proxy_context_put_termqueue(proxy_context_t *my)
{
	if (llist_append_nb(my->pool->terminated_queue, my)) {
		//mylog(L_ERR, "put context to terminated queue failed");
		fprintf(stderr, "put context to terminated queue failed\n");
	}
	//mylog(L_ERR, "put context to terminated queue success");
	fprintf(stderr, "%u : put context to terminated queue success, %p\n", gettid(), my);
	return 0;
}

int proxy_context_put_runqueue(proxy_context_t *my)
{
	if (llist_append_nb(my->pool->run_queue, my)) {
		//mylog(L_ERR, "put context to run queue failed");
		fprintf(stderr, "put context to run queue failed\n");
	}
	//mylog(L_ERR, "put context to run queue success");
	fprintf(stderr, "%u : put context to run queue success, %p\n", gettid(), my);
	return 0;
}
int proxy_context_get_epollfd(proxy_context_t *my)
{
	struct epoll_event ev;
	int ret;

	ev.data.ptr = my;
	switch (my->state) {
		case STATE_CLOSE:
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_DEL, my->client_conn->sd, NULL);
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_DEL, my->server_conn->sd, NULL);
			break;
		case STATE_TERM:
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_DEL, my->client_conn->sd, NULL);
			epoll_ctl(my->pool->epoll_io, EPOLL_CTL_DEL, my->server_conn->sd, NULL);
			break;
		default:
			break;
	}
	return 0;
}

int proxy_context_put_epollfd(proxy_context_t *my)
{
	struct epoll_event ev;
	int ret;

	ev.data.ptr = my;
	switch (my->state) {
		case STATE_ACCEPT_CREATE:
			ev.events = EPOLLIN | EPOLLONESHOT;  
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_ADD, my->listen_sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "accept create add event error %s", strerror(errno));
			} 
			break;
		case STATE_ACCEPT:
			ev.events = EPOLLIN | EPOLLONESHOT;  
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->listen_sd, &ev);
			//epoll_ctl(my->pool->epoll_accept, EPOLL_CTL_MOD, my->listen_sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "accept mod event error %s", strerror(errno));
			} 
			break;
		case STATE_READHEADER:
			ev.events = EPOLLIN | EPOLLONESHOT;
			//epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_ADD, my->client_conn->sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "readheader add event error %s", strerror(errno));
			} 
			break;
		case STATE_CONNECTSERVER:
			ev.events = EPOLLOUT | EPOLLONESHOT;
			//epoll_ctl(my->pool->epoll_io, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
			epoll_ctl(my->epoll_context, EPOLL_CTL_ADD, my->server_conn->sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "connectserver add event error %s", strerror(errno));
			} 
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
			/* watch event of server connection sd */
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "iowait mod server event error %s", strerror(errno));
			} 

			ev.events = EPOLLONESHOT;
			/* If buffer is not full*/
			if (my->s2c_wactive) {
				ev.events |= EPOLLIN;
			}
			if (buffer_nbytes(my->s2c_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			/* watch event of client connection sd */
			epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			if (ret == -1) {
				//mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
			} 
			break;
		default:
			//mylog(L_ERR, "unknown state");
			fprintf(stderr, "unknown state\n");
			//abort();
			//			proxy_context_put_runqueue(my);
			break;
	}
	ev.data.ptr = my;
	ev.events = EPOLLOUT|EPOLLIN|EPOLLONESHOT;
	epoll_ctl(my->poll->epoll_pool, EPOLL_CTL_MOD, my->epoll_context, &ev);
	return 0;
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	proxy_context_t *newproxy;
	connection_t *client_conn;
	int ret;

	//mylog(L_ERR, "begin driver accept");
	fprintf(stderr, "begin driver accept\n");

	my->state = STATE_ACCEPT;

	proxy_context_put_epollfd(my);

	ret = connection_accept_nb(&client_conn, my->listen_sd);
	if (ret) {
		if (ret==EAGAIN || ret==EINTR) {
			//mylog(L_ERR, "nonblock accept, wait for next turn");
			fprintf(stderr, "nonblock accept, wait for next turn\n");
			proxy_context_put_epollfd(my);
			return -1;
		} else {
			fprintf(stderr, "accept failed\n");
			//mylog(L_ERR, "accept failed");
			return -1;
		}
	}
	// TODO: Check pool configure to refuse redundent connections.
	if (my->pool->nr_idle + my->pool->nr_busy +1 > my->pool->nr_total) {
		fprintf(stderr, "create new context from accept\n");
		//mylog(L_ERR, "create new context from accept");
		newproxy = proxy_context_new(my->pool, client_conn);
		newproxy->state = STATE_READHEADER;
		proxy_context_put_epollfd(newproxy);
	} else {
		fprintf(stderr, "refuse connection\n");
		//mylog(L_ERR, "refuse connection");
		connection_close_nb(client_conn);
	}
	return 0;
}

static int proxy_context_driver_readheader(proxy_context_t *my)
{
	ssize_t len;
	char *server_ip, *tmp;
	int server_port;
	char *hdrbuf = my->http_header_buffer;
	int pos;

	//mylog(L_ERR, "begin driver readheader");
	fprintf(stderr, "begin driver readheader\n");

	memset(my->http_header_buffer, 0, HEADERSIZE);
	my->http_header_buffer_pos = 0;

	for (pos = 0; pos<HEADERSIZE; ) {
		len = connection_recv_nb(my->client_conn, 
				my->http_header_buffer + my->http_header_buffer_pos, HEADERSIZE - pos - 1);
		if (len<0) {
			//mylog(L_ERR, "read header failed");
			fprintf(stderr, "read header failed\n");
			my->errlog_str = "Header is too long.";
			my->state = STATE_ERR;
			proxy_context_put_runqueue(my);
			return -1;
		}

		my->http_header_buffer_pos += len;
		pos += len;

		//TODO: write body data to c2s_buf
		if (strstr(hdrbuf, "\r\n\r\n") || strstr(hdrbuf, "\n\n")) {
			//mylog(L_ERR, "deal header data");
			fprintf(stderr, "deal header data\n");
			if (buffer_write(my->c2s_buf, hdrbuf, pos)<0) {
				fprintf(stderr, "write header to c2s buffer failed\n");
				//mylog(L_ERR, "write header to c2s buffer failed");
				my->errlog_str = "The first write of c2s_buf failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			if (http_header_parse(&my->http_header, hdrbuf) < 0) {
				fprintf(stderr, "parse header failed\n");
				//mylog(L_ERR, "parse header failed");
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}

			server_ip = malloc(my->http_header.host.size + 1);
			if (server_ip == NULL) {
				//mylog(L_ERR, "no memory for header host");
				fprintf(stderr, "no memory for header host\n");
				my->errlog_str = "Http header read server ip failed.";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}
			memcpy(server_ip, my->http_header.host.ptr, my->http_header.host.size);
			server_ip[my->http_header.host.size] = '\0';
			tmp = strchr(server_ip, ':');
			if (tmp) {
				*tmp = '\0';
				server_port = atoi(tmp + 1);
			} else {
				server_port = 80;
			}

			/*
			 * hostname or ip
			 */
			if ((inet_addr(server_ip)) == INADDR_NONE) {
				struct hostent *h;

				h = gethostbyname(server_ip);
				if (h == NULL) {
					//mylog(L_ERR, "parse header failed: gethostbyname error");
					fprintf(stderr, "parse header failed: gethostbyname error\n");
					my->errlog_str = "Http header parse failed.";
					my->state = STATE_ERR;
					proxy_context_put_runqueue(my);
					return -1;
				}

				my->server_ip = inet_ntoa(*((struct in_addr *)(h->h_addr_list[0])));
				free(server_ip);
			} else {
				my->server_ip = server_ip;
			}
			my->server_port = server_port;


			my->state = STATE_CONNECTSERVER;
			proxy_context_put_runqueue(my);
			return 0;
		}
	}

	//should never reach here
	return -1;
}

/*
 * Try to build the server size connection.
 * After this state, both connection are r/w ready.
 */
static int proxy_context_driver_connectserver(proxy_context_t *my)
{
	int err;

	//mylog(L_ERR, "begin driver connectserver");
	fprintf(stderr, "begin driver connectserver\n");

	err = connection_connect_nb(&my->server_conn, my->server_ip, my->server_port);

	if (err==0 || err==EISCONN) {
		//mylog(L_ERR, "err is %d, change state into IOWAIT", err);
		fprintf(stderr, "err is %d, change state into IOWAIT\n", err);
		my->state = STATE_IOWAIT;
		proxy_context_put_epollfd(my);
	} else if (err != EINPROGRESS && err != EALREADY) {
		//mylog(L_ERR, "some error occured, error is %d(%s) reject client", err, strerror(err));
		fprintf(stderr, "some error occured, error is %d(%s) reject client\n", err, strerror(err));
		my->state = STATE_REJECTCLIENT;
		proxy_context_put_runqueue(my);
	} else {
		//mylog(L_ERR, "err is EINPROGRESS, will retry connect server");
		fprintf(stderr, "err is EINPROGRESS, will retry connect server\n");
		proxy_context_put_epollfd(my); //EINPROGRESS
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
	//server_state_set();
	return 0;
}

/*
 * Process proxy context timeout.
 */
int proxy_context_timedout(proxy_context_t *my)
{
	my->state = STATE_REJECTCLIENT;
	proxy_context_put_termqueue(my);
	return 0;
}

/*
 * Test if the server side was timed out.
 * Client timeout not supported yet.
 */
int is_proxy_context_timedout(proxy_context_t *my)
{
	struct timeval now, past, diff;

	if (my->server_conn) {
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
int proxy_context_driver_iowait(proxy_context_t *my, int io)
{
	int res;

	fprintf(stderr, "%u : into iowait %p\n", gettid(), my);
	if (io & EPOLLIN) {
		//mylog(L_ERR, "receiving from server");
		fprintf(stderr, "receiving from server\n");
		/* recv from server */
		while (my->s2c_wactive) {
			res = connection_recvv_nb(my->server_conn, my->s2c_buf, DATA_BUFMAX);
			if (res < 0) { 
				if (errno == EAGAIN || errno == EINTR) {
					//mylog(L_WARNING, "nonblock receive data from server failed, try again");
					fprintf(stderr, "nonblock receive data from server failed, try again\n");
					break; //non-block
				} else {
					/* generate error message to client */
					//mylog(L_ERR, "receive data from server failed, close connection");
					fprintf(stderr, "receive data from server failed, close connection\n");
					proxy_context_connection_failed(my);
					my->state = STATE_TERM;
					proxy_context_get_epollfd(my);
					proxy_context_put_termqueue(my);
					return 0;
				}
			}

			if (res == 0) {
				/* server close */
				//mylog(L_WARNING, "server close");
				fprintf(stderr, "server close\n");
				break;
			}

			//mylog(L_ERR, "recv %d from server", res);
			fprintf(stderr, "recv %d from server\n", res);
			if (my->s2c_buf->bufsize == my->s2c_buf->max) {
				my->s2c_wactive = 0;
				break;
			}
		}

		//mylog(L_ERR, "receiving from client");
		fprintf(stderr, "receiving from client\n");
		/* recv from client */
		while (my->c2s_wactive) {
			res = connection_recvv_nb(my->client_conn, my->c2s_buf, DATA_BUFSIZE);
			if (res < 0) { 
				if (errno == EAGAIN || errno == EINTR) {
					//mylog(L_WARNING, "nonblock receive data from client failed, try again");
					fprintf(stderr, "nonblock receive data from client failed, try again\n");
					break; //non-block
				} else {
					//mylog(L_ERR, "receive data from client failed, close connection");
					fprintf(stderr, "receive data from client failed, close connection\n");
					my->state = STATE_TERM;
					proxy_context_get_epollfd(my);
					proxy_context_put_termqueue(my);
					return 0;
				}
			}
			if (res == 0) {
				/* client close */
				//mylog(L_ERR, "client is closed, close all connection");
				fprintf(stderr, "%u : client is closed, close all connection, STATE_CLOSE, %p\n", gettid(), my);
				my->state = STATE_CLOSE;
				/* remove my from io event pool */
				proxy_context_get_epollfd(my);
				proxy_context_put_runqueue(my);
				return 0;
			}

			//mylog(L_ERR, "recv %d from client", res);
			fprintf(stderr, "recv %d from client\n", res);
			if (my->c2s_buf->bufsize == my->c2s_buf->max) {
				my->c2s_wactive = 0;
				break;
			}
		}
	}

	if (io & EPOLLOUT) {
		//mylog(L_ERR, "sending to client");
		fprintf(stderr, "sending to client\n");
		/* send to client */
		while (buffer_nbytes(my->s2c_buf) > 0) {
			res = connection_sendv_nb(my->client_conn, my->s2c_buf, DATA_SENDSIZE);
			if (res < 0) { 
				if (errno == EAGAIN || errno == EINTR) {
					//mylog(L_WARNING, "nonblock send data to client failed, try again");
					fprintf(stderr, "nonblock send data to client failed, try again\n");
					break; //non-block
				} else {
					// client error
					//mylog(L_ERR, "send data from client failed, close connection");
					fprintf(stderr, "send data from client failed, close connection\n");
					my->state = STATE_TERM;
					proxy_context_get_epollfd(my);
					proxy_context_put_termqueue(my);
					return 0;
				}
			}
			if (res == 0) {
				break;
			}
			//mylog(L_ERR, "send %d to client", res);
			fprintf(stderr, "send %d to client\n", res);
			if (my->s2c_buf->bufsize < my->s2c_buf->max) {
				my->s2c_wactive = 1;
			}
		}

		//mylog(L_ERR, "sending to server");
		fprintf(stderr, "sending to server\n");
		/* send to server */
		while (buffer_nbytes(my->c2s_buf) > 0) {
			res = connection_sendv_nb(my->server_conn, my->c2s_buf, DATA_SENDSIZE);
			if (res < 0) { 
				if (errno == EAGAIN || errno == EINTR) {
					//mylog(L_WARNING, "nonblock send data to server failed, try again");
					fprintf(stderr, "nonblock send data to server failed, try again\n");
					break; //non-block
				} else {
					/* generate error message to client */
					//mylog(L_ERR, "send data to server failed, close connection");
					fprintf(stderr, "send data to server failed, close connection\n");
					proxy_context_connection_failed(my);
					my->state = STATE_TERM;
					proxy_context_get_epollfd(my);
					proxy_context_put_termqueue(my);
					return 0;
				}
			}
			if (res == 0) {
				break;
			}
			//mylog(L_ERR, "sent %d to server", res);
			fprintf(stderr, "sent %d to server\n", res);
			if (my->c2s_buf->bufsize < my->c2s_buf->max) {
				my->c2s_wactive = 1;
			}
		}
	}

	fprintf(stderr, "%u : outoff iowait %p\n", gettid(), my);
	proxy_context_put_epollfd(my);

	return 0;
}

int proxy_context_driver_close(proxy_context_t *my)
{
	//mylog(L_ERR, "begin driver close");
	fprintf(stderr, "%u : begin driver close, %p\n", gettid(), my);
	return proxy_context_delete(my);
}

int proxy_context_driver_rejectclient(proxy_context_t *my)
{
	ssize_t res;
	char *err_msg= "connet to server failed";
	size_t len = strlen(err_msg);

	//mylog(L_ERR, "begin driver rejectclient");
	fprintf(stderr, "%u : begin driver rejectclient, %p\n", gettid(), my);

	res = buffer_write(my->s2c_buf, err_msg, len);
	if (res != len) {
		//mylog(L_ERR, "write connect message to client failed");
		fprintf(stderr, "write connect message to client failed\n");
	}
	while (1) {
		res = connection_sendv_nb(my->client_conn, my->s2c_buf, DATA_SENDSIZE);
		if (res < 0) { 
			if (errno == EAGAIN || errno == EINTR) {
				//mylog(L_WARNING, "nonblock send data to client failed, try again");
				fprintf(stderr, "nonblock send data to client failed, try again\n");
			} else {
				// client error
				//mylog(L_ERR, "send data from client failed, close connection");
				fprintf(stderr, "send data from client failed, close connection\n");
				break;
			}   
		} else {
			break;
		}
	}

	my->state = STATE_TERM;
	proxy_context_get_epollfd(my);
	proxy_context_put_termqueue(my);

	return 0;
}

int proxy_context_driver_error(proxy_context_t *my)
{
	int ret;
	//mylog(L_ERR, "context driver error occurred");
	fprintf(stderr, "context driver error occurred\n");
	my->state = STATE_TERM;
	proxy_context_get_epollfd(my);
	proxy_context_put_termqueue(my);

	return ret;
}

int proxy_context_driver_term(proxy_context_t *my)
{
	int ret;
	//mylog(L_ERR, "context driver terminate");
	fprintf(stderr, "%u : context driver terminate, %p\n", gettid(), my);
	//TODO: send some error message to client

	return proxy_context_delete(my);
}
/*
 * Main driver of proxy context FSA
 */
int proxy_context_driver(proxy_context_t *my, int io)
{
	int ret;
	switch (my->state) {
		case STATE_ACCEPT_CREATE:
			ret = proxy_context_driver_accept(my);
			break;
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
			ret = proxy_context_driver_iowait(my, io);
			break;
		case STATE_REJECTCLIENT:
			ret = proxy_context_driver_rejectclient(my);
			break;
		case STATE_CLOSE:
			ret = proxy_context_driver_close(my);
			break;
		case STATE_ERR:
			ret = proxy_context_driver_error(my);
			break;
		case STATE_TERM:
			ret = proxy_context_driver_term(my);
			break;
		default:
			break;
	}
	//mylog(L_ERR, "{CAUTION} leaving drive");
	fprintf(stderr, "%u : {CAUTION} leaving drive, %p\n", gettid(), my);
	return ret;
}

