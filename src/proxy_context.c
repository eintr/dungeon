#include <stdio.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "proxy_context.h"
#include "aa_syscall.h"
#include "aa_state_dict.h" 
#include "aa_http_status.h"
#include "aa_conf.h"
#include "aa_err.h"
#include "aa_log.h"

proxy_context_t *proxy_context_new_accepter(proxy_pool_t *pool)
{
	proxy_context_t *newnode = NULL;
	struct epoll_event ev;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		memset(newnode, 0, sizeof(proxy_context_t));
		newnode->pool = pool;
		newnode->state = STATE_ACCEPT;
		newnode->listen_sd = pool->original_listen_sd;
/*		Why use dup() here? Is it nessesary??
 *		newnode->listen_sd = dup(pool->original_listen_sd);
		if (newnode->listen_sd<0) {
			mylog(L_ERR, "dup(listen_sd) failed");
			goto drop_and_fail;
		}*/
		newnode->epoll_context = epoll_create(1);
		newnode->timer = -1;
		newnode->client_conn = NULL;
		newnode->http_header_buffer = NULL;
		newnode->http_header_buffer_pos = 0;
		newnode->server_conn = NULL;
		newnode->s2c_buf = NULL;
		newnode->c2s_buf = NULL;
		newnode->s2c_wactive = 1;
		newnode->c2s_wactive = 1;
		newnode->errlog_str = "No error.";
		newnode->set_dict = 0;
	
		ev.data.ptr = newnode;
		ev.events = EPOLLOUT|EPOLLIN;
		epoll_ctl(newnode->epoll_context, EPOLL_CTL_ADD, newnode->listen_sd, &ev);

		ev.events = EPOLLOUT|EPOLLIN|EPOLLONESHOT;
		epoll_ctl(pool->epoll_pool, EPOLL_CTL_ADD, newnode->epoll_context, &ev);
	}
	return newnode;
}

static proxy_context_t *proxy_context_new(proxy_pool_t *pool, connection_t *client_conn)
{
	proxy_context_t *newnode = NULL;
	int timeout;

	newnode = malloc(sizeof(*newnode));
	if (newnode) {
		memset(newnode, 0, sizeof(proxy_context_t));
		newnode->pool = pool;
		newnode->state = STATE_READHEADER;
		newnode->listen_sd = -1;
		newnode->client_conn = client_conn;
		newnode->epoll_context = epoll_create(1);
		newnode->timer = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
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

		timeout = conf_get_receive_timeout(); 
		newnode->server_r_timeout_tv.tv_sec = timeout / 1000;
		newnode->server_r_timeout_tv.tv_usec = timeout % 1000;
		newnode->client_r_timeout_tv.tv_sec = timeout / 1000;
		newnode->client_r_timeout_tv.tv_usec = timeout % 1000;

		timeout = conf_get_send_timeout();
		newnode->server_s_timeout_tv.tv_sec = timeout / 1000;
		newnode->server_s_timeout_tv.tv_usec = timeout % 1000;
		newnode->client_s_timeout_tv.tv_sec = timeout / 1000;
		newnode->client_s_timeout_tv.tv_usec = timeout % 1000;

		timeout = conf_get_connect_timeout();
		newnode->server_connect_timeout_tv.tv_sec = timeout / 1000;
		newnode->server_connect_timeout_tv.tv_usec = timeout % 1000;
		
		newnode->s2c_wactive = 1;
		newnode->c2s_wactive = 1;
		newnode->errlog_str = "No error.";
		newnode->set_dict = 1;
	}
	return newnode;

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
	mylog(L_DEBUG, "In proxy_context_delete");
	if (my == NULL) {
		return -1;
	}
	if (my->state != STATE_TERM && my->state != STATE_CLOSE && my->state != STATE_ERR) {
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
	my->c2s_buf = NULL;
	if (buffer_delete(my->s2c_buf)) {
		mylog(L_ERR, "delete s2c buffer failed");
	}
	my->s2c_buf = NULL;

	free(my->http_header_buffer);
	if (my->epoll_context) {
		close(my->epoll_context);
	}

	if (my->timer != -1) {
		close(my->timer);
	}

	if (my->client_conn && connection_close_nb(my->client_conn)) {
		mylog(L_ERR, "close client connectin failed");
	}
	my->client_conn = NULL;
	if (my->server_conn && connection_close_nb(my->server_conn)) {
		mylog(L_ERR, "close server connectin failed");
	}
	my->server_conn = NULL;

	close(my->listen_sd);
	mylog(L_DEBUG, "in context close my is %p", my);

	free(my);

	return 0;
}

int proxy_context_put_termqueue(proxy_context_t *my)
{
	while (llist_append_nb(my->pool->terminated_queue, my)) {
		mylog(L_ERR, "put context to terminated queue failed");
	}
	mylog(L_DEBUG, "put context to terminated queue success, %p", my);
	return 0;
}

int proxy_context_put_runqueue(proxy_context_t *my)
{
	int ret;
	while ((ret = llist_append_nb(my->pool->run_queue, my))) {
		mylog(L_ERR, "ret is %d, put context to run queue failed, %p", ret, my);
	}
	mylog(L_DEBUG, "put context to run queue success, %p", my);
	return 0;
}

int proxy_context_retrieve_epollfd(proxy_context_t *my)
{
	switch (my->state) {
		case STATE_CLOSE:
		case STATE_TERM:
			epoll_ctl(my->pool->epoll_pool, EPOLL_CTL_DEL, my->epoll_context, NULL);
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
		case STATE_ACCEPT:
			ev.events = EPOLLIN;  
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->listen_sd, &ev);
			if (ret == -1) {
				mylog(L_ERR, "[accept] : mod listen sd to epoll_context failed");
			} 
			break;
		case STATE_READHEADER:
			/* timeout */
			ev.data.u32 = 2;
			ev.events = EPOLLIN;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->timer, &ev);
			if (ret == -1) {
				mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
			} 
			/* watch client fd */
			ev.data.u32 = 1;
			ev.events = EPOLLIN;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
			if (ret == -1) {
				mylog(L_ERR, "readheader add event error %s", strerror(errno));
			} 
			break;
		case STATE_CONNECTSERVER:
			/* timeout */
			ev.data.u32 = 2;
			ev.events = EPOLLIN;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->timer, &ev);
			if (ret == -1) {
				mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
			} 
			/* watch server fd */
			ev.data.u32 = 1;
			ev.events = EPOLLOUT;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_ADD, my->server_conn->sd, &ev);
			if (ret == -1) {
				mylog(L_ERR, "connectserver add event error %s", strerror(errno));
			} 
			break;
		case STATE_IOWAIT:
			/* timeout */
			ev.data.u32 = 2;
			ev.events = EPOLLIN;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->timer, &ev);
			if (ret == -1) {
				mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
			} 

			/* watch event of server connection sd */
			ev.data.u32 = 0;
			/* If buffer is not full*/
			if (my->c2s_wactive) {
				ev.events = EPOLLIN;
			}
			if (buffer_nbytes(my->c2s_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			if (my->server_conn) {
				ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->server_conn->sd, &ev);
				if (ret == -1) {
					mylog(L_ERR, "iowait mod server event error %s", strerror(errno));
				} 
			}

			/* watch event of client connection sd */
			ev.data.u32 = 1;
			/* If buffer is not full*/
			if (my->s2c_wactive) {
				ev.events = EPOLLIN;
			}
			if (buffer_nbytes(my->s2c_buf)>0) {
				ev.events |= EPOLLOUT;
			}
			if (my->client_conn) {
				ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->client_conn->sd, &ev);
				if (ret == -1) {
					mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
				} 
			}
			break;
		case STATE_CONN_PROBE:
			/* timeout */
			ev.data.u32 = 2;
			ev.events = EPOLLIN;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_MOD, my->timer, &ev);
			if (ret == -1) {
				mylog(L_ERR, "iowait mod client event error %s", strerror(errno));
			} 

			/* watch server fd */
			ev.data.u32 = 1;
			ev.events = EPOLLOUT;
			ret = epoll_ctl(my->epoll_context, EPOLL_CTL_ADD, my->server_conn->sd, &ev);
			if (ret == -1) {
				mylog(L_ERR, "connectserver add event error %s", strerror(errno));
			}   
			break;
		default:
			mylog(L_ERR, "unknown state");
			//abort();
			//			proxy_context_put_runqueue(my);
			break;
	}

	if (my->state != STATE_READHEADER) {
		ev.data.ptr = my;
		ev.events = EPOLLIN | EPOLLONESHOT;
		ret = epoll_ctl(my->pool->epoll_pool, EPOLL_CTL_MOD, my->epoll_context, &ev);
		if (ret == -1) {
			mylog(L_ERR, "mod my->epoll_context failed: %s", strerror(errno));
		}
	}
	return 0;
}

static int proxy_context_generate_message(buffer_list_t *bl, char *msg)
{
	int len = strlen(msg);
	int res;

	mylog(L_DEBUG, "in generate message");

	res = buffer_write(bl, msg, len);
	if (res != len) {
		mylog(L_ERR, "write message to buffer failed");
	}

	return res;
}

static int proxy_context_settimer(int timer, struct timeval *tv)
{
	struct itimerspec its;
	int ret;

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = tv->tv_sec;
	its.it_value.tv_nsec = tv->tv_usec * 1000 * 1000;

	ret = timerfd_settime(timer, 0, &its, NULL);
	mylog(L_DEBUG, "set time ret is %d", ret);

	return 0;
}

static int proxy_context_driver_accept(proxy_context_t *my)
{
	proxy_context_t *newproxy;
	connection_t *client_conn;
	struct epoll_event ev;
	int ret;

	mylog(L_DEBUG, "begin driver accept");

	while (1) {
		ret = connection_accept_nb(&client_conn, my->listen_sd);
		if (ret) {
			if (ret==EAGAIN || ret==EINTR) {
				mylog(L_DEBUG, "nonblock accept, wait for next turn");
				break;
			} else {
				mylog(L_DEBUG, "accept failed.");
				break;
			}
		}
		// TODO: Check pool configure to refuse redundent connections.
		if (my->pool->nr_idle + my->pool->nr_busy +1 > my->pool->nr_total) {
			mylog(L_DEBUG, "create new context from accept");
			newproxy = proxy_context_new(my->pool, client_conn);
			newproxy->state = STATE_READHEADER;
		
			proxy_context_settimer(newproxy->timer, &newproxy->client_r_timeout_tv);

			ev.data.u32 = 2;
			ev.events = EPOLLIN;
			ret = epoll_ctl(newproxy->epoll_context, EPOLL_CTL_ADD, newproxy->timer, &ev);
			if (ret == -1) {
				mylog(L_ERR, "add timer event error %s", strerror(errno));
			} 
			
			/* watch client fd */
			ev.data.u32 = 1;
			ev.events = EPOLLIN;
			ret = epoll_ctl(newproxy->epoll_context, EPOLL_CTL_ADD, newproxy->client_conn->sd, &ev);
			if (ret == -1) {
				mylog(L_ERR, "readheader add event error %s", strerror(errno));
			} 

			ev.data.ptr = newproxy;
			ev.events = EPOLLIN|EPOLLONESHOT;
			ret = epoll_ctl(newproxy->pool->epoll_pool, EPOLL_CTL_ADD, newproxy->epoll_context, &ev);
			if (ret == -1) {
				mylog(L_DEBUG, "add newproxy epoll_context to epoll_pool failed: %s", strerror(errno));
			}
		} else {
			mylog(L_WARNING, "Connection refused.");
			connection_close_nb(client_conn);
		}
	}

	proxy_context_put_epollfd(my);

	return 0;
}

static int proxy_context_driver_readheader(proxy_context_t *my)
{
	ssize_t len;
	char *hdrbuf = my->http_header_buffer;
	int i, pos;
	struct timeval tv;
	struct epoll_event events[2];
	int nfds;

	mylog(L_DEBUG, "state is readheader");
	
	nfds = epoll_wait(my->epoll_context, events, 2, 0);
	
	/* disarm timer */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	proxy_context_settimer(my->timer, &tv);
	
	if (nfds < 0) {
		mylog(L_WARNING, "driver readheader epoll_wait return %d", nfds);
		proxy_context_put_epollfd(my);
	} 
	
	for (i = 0; i < nfds; i++) {
		if (events[i].data.u32 == 2) {
			/* timerfd out */
			my->state = STATE_TERM;
			proxy_context_put_termqueue(my);
			return 0;
		}

		memset(my->http_header_buffer, 0, HEADERSIZE);
		my->http_header_buffer_pos = 0;

		for (pos = 0; pos<HEADERSIZE; ) {
			len = connection_recv_nb(my->client_conn, 
					my->http_header_buffer + my->http_header_buffer_pos, HEADERSIZE - pos - 1);
			if (len<0) {
				if (-len==EAGAIN || -len==EINTR) {
					proxy_context_settimer(my->timer, &my->client_r_timeout_tv);
					proxy_context_put_epollfd(my);
					return 0;
				}
				mylog(L_ERR, "read header failed, errno=%d", -len);
				my->errlog_str = "connection_recv_nb()";
				my->state = STATE_ERR;
				proxy_context_put_runqueue(my);
				return -1;
			}

			my->http_header_buffer_pos += len;
			pos += len;

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

				my->state = STATE_PARSEHEADER;
				proxy_context_put_runqueue(my);
				return 0;
			}
		}
	}
	//should never reach here
	return -1;
}

/*
 * Parse header
 */
static int proxy_context_driver_parseheader(proxy_context_t *my)
{
	char *host, *tmp;
	int server_port;
	struct epoll_event ev;

	mylog(L_DEBUG, "state is parseheader");

	if (http_header_parse(&my->http_header, my->http_header_buffer) < 0) {
		mylog(L_ERR, "parse header failed");
		my->errlog_str = "Http header parse failed.";
		my->state = STATE_ERR;
		proxy_context_put_runqueue(my);
		return -1;
	}

	host = malloc(my->http_header.host.size + 1);
	if (host == NULL) {
		mylog(L_ERR, "no memory for header host");
		my->errlog_str = "Http header read server ip failed.";
		my->state = STATE_ERR;
		proxy_context_put_runqueue(my);
		return -1;
	}
	memcpy(host, my->http_header.host.ptr, my->http_header.host.size);
	host[my->http_header.host.size] = '\0';

	mylog(L_DEBUG, "before state get host is %s", host);
	struct server_state_st *ss = server_state_get(host);

	/* extract port */
	tmp = strchr(host, ':');
	if (tmp) {
		*tmp = '\0';
		server_port = atoi(tmp + 1);
	} else {
		server_port = 80;
	}

	my->server_port = server_port;

	/* get from state dict */
	if (ss) {
		char *hostip = malloc(32);
		if (hostip == NULL) {
			mylog(L_ERR, "no memory for header host");
			free(host);
			my->errlog_str = "Http header read server ip failed.";
			my->state = STATE_ERR;
			proxy_context_put_runqueue(my);
			return -1;
		}
		inet_ntop(AF_INET, &ss->serverinfo.saddr.sin_addr, hostip, 32);
		my->server_ip = hostip;	

		if (ss->current_state == SS_UNKNOWN || ss->current_state == SS_OK) {
			mylog(L_DEBUG, "in readheader SS_UNKNOWN is %d, SS_OK is %d", SS_UNKNOWN, SS_OK);
			mylog(L_DEBUG, "in readheader current state is %d", ss->current_state);
			if (ss->current_state == SS_OK) {
				my->set_dict = 0;
			}
			free(host);
		} else {
			/* state is SS_CONN_FAILURE, assume no dns failure */
			proxy_context_t *c;
			int ret;

			mylog(L_DEBUG, "in readheader current state is SS_CONN_FAILURE");

			my->state = STATE_CONN_PROBE;
			free(host);

			mylog(L_DEBUG, "fork new context from");

			c = proxy_context_new(my->pool, my->client_conn);
			if (c) {
				proxy_context_generate_message(c->s2c_buf, HTTP_503);
				c->state = STATE_IOWAIT;

				ev.data.ptr = c;
				ev.events = EPOLLIN | EPOLLOUT;
				ret = epoll_ctl(c->epoll_context, EPOLL_CTL_ADD, c->client_conn->sd, &ev);
				if (ret == -1) {
					mylog(L_ERR, "parseheader add event error %s", strerror(errno));
				} 

				ev.events = EPOLLIN | EPOLLONESHOT;
				ret = epoll_ctl(c->pool->epoll_pool, EPOLL_CTL_ADD, c->epoll_context, &ev);
				if (ret == -1) {
					mylog(L_DEBUG, "add new context to epoll_pool failed : %s", strerror(errno));
				}

				mylog(L_DEBUG, "new context is created, it is %p", c);
				my->client_conn = NULL;
			}

			proxy_context_put_runqueue(my);
			return 0;
		}
	} else {
		/* 
		 * cannot get record from dict 
		 */
		mylog(L_DEBUG, "in readheader cannot get this record from dict");
		/* hostname or ip */
		server_info_t sinfo;

		if ((inet_addr(host)) == INADDR_NONE) {
			/* need dns resolve */
			struct hostent *h;

			h = gethostbyname(host);
			if (h == NULL) {
				mylog(L_ERR, "parse header failed: gethostbyname error");
				my->errlog_str = "Http header parse failed.";
				my->state = STATE_ERR;
				//memset(sinfo.hostname, 0, sizeof(sinfo.hostname));
				//strncpy(sinfo.hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
				//sinfo.hostname[my->http_header.host.size] = '\0';
				//server_state_add_default(&sinfo, SS_DNS_FAILURE);
				proxy_context_put_runqueue(my);
				return -1;
			}

			my->server_ip = inet_ntoa(*((struct in_addr *)(h->h_addr_list[0])));
			free(host);
		} else {
			my->server_ip = host;
		}

		/* add state to dict */
		memset(sinfo.hostname, 0, sizeof(sinfo.hostname));
		strncpy(sinfo.hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
		sinfo.hostname[my->http_header.host.size] = '\0';
		sinfo.saddr.sin_family = AF_INET;
		inet_pton(AF_INET, my->server_ip, &sinfo.saddr.sin_addr); 
		server_state_add_default(&sinfo, SS_UNKNOWN);
	}

	my->state = STATE_CONNECTSERVER;
	proxy_context_put_runqueue(my);

	return 0;
}

/*
 * Try to build the server size connection.
 * After this state, both connection are r/w ready.
 */
static int proxy_context_driver_connectserver(proxy_context_t *my)
{
	int err, ret;
	struct epoll_event events[2];
	struct timeval tv;
	int nfds;
	int i;

	nfds = epoll_wait(my->epoll_context, events, 2, 0);
	
	/* disarm timer */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	proxy_context_settimer(my->timer, &tv);
	
	if (nfds < 0) {
		mylog(L_WARNING, "driver connectserver epoll_wait return %d", nfds);
		proxy_context_put_epollfd(my);
	}

	if (nfds == 0) {
		ret = 1;
	}

	/* if fd event happend ignore timeout */
	for (i = 0; i < nfds; i++) {
		if (events[i].data.u32 == 2) {
			ret = 0;
		} else {
			ret = 1;
			break;
		}
	}
	

	if (ret == 0) {
		/* timeout, close server conn immediately */
		if (my->server_conn && connection_close_nb(my->server_conn)) {
			mylog(L_ERR, "close server connectin failed");
		}
		my->server_conn = NULL;

		proxy_context_generate_message(my->s2c_buf, HTTP_504);

		my->state = STATE_IOWAIT;
		proxy_context_settimer(my->timer, &my->client_s_timeout_tv);

		/* set dict */
		char hostname[HOSTNAMESIZE];
		strncpy(hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
		hostname[my->http_header.host.size] = '\0';
		server_state_set_state(hostname, SS_CONN_FAILURE);

		proxy_context_put_epollfd(my);
	} else {
		/* connect server */
		mylog(L_DEBUG, "begin driver connectserver");

		err = connection_connect_nb(&my->server_conn, my->server_ip, my->server_port);

		if (err==0 || err==EISCONN) {
			mylog(L_ERR, "err is %d, change state into IOWAIT", err);
			my->state = STATE_IOWAIT;
			if (my->set_dict) {
				char hostname[HOSTNAMESIZE];
				strncpy(hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
				hostname[my->http_header.host.size] = '\0';
				server_state_set_state(hostname, SS_OK);
			}  

			proxy_context_settimer(my->timer, &my->server_r_timeout_tv);

			proxy_context_put_epollfd(my);
		} else if (err != EINPROGRESS && err != EALREADY) {
			mylog(L_ERR, "some error occured, error is %d(%s) reject client", err, strerror(err));
			/* close server conn immediately */
			if (my->server_conn && connection_close_nb(my->server_conn)) {
				mylog(L_ERR, "close server connectin failed");
			}
			my->server_conn = NULL;

			my->state = STATE_REJECTCLIENT;

			/* set dict */
			char hostname[HOSTNAMESIZE];
			strncpy(hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
			hostname[my->http_header.host.size] = '\0';
			server_state_set_state(hostname, SS_CONN_FAILURE);

			proxy_context_put_runqueue(my);
		} else {
			mylog(L_ERR, "err is EINPROGRESS, will retry connect server");
			proxy_context_settimer(my->timer, &my->server_connect_timeout_tv);
			proxy_context_put_epollfd(my); //EINPROGRESS
		}
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
int proxy_context_driver_iowait(proxy_context_t *my)
{
	int res;
	int nfds;
	struct epoll_event events[3];
	int i;
	struct timeval tv;

	nfds = epoll_wait(my->epoll_context, events, 3, 0);
	mylog(L_DEBUG, "in driver epoll_wait return %d", nfds);

	if (nfds <= 0) {
		mylog(L_DEBUG, "in driver epoll_wait return %d", nfds);
		goto putfd;
	} 

	/* disarm timer */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	proxy_context_settimer(my->timer, &tv);

	for (i = 0; i < nfds; i++) {
		/* timeout */
		if (events[i].data.u32 == 2) {
			/* timerfd out, close server conn immediately */
			mylog(L_DEBUG, "iowait timeout happend");
			proxy_context_generate_message(my->s2c_buf, HTTP_504);

			my->state = STATE_IOWAIT;

			proxy_context_settimer(my->timer, &my->client_s_timeout_tv);
			proxy_context_put_epollfd(my);

			return 0;
		}

		/* server conn */
		if (events[i].data.u32 == 0) {
			/* recv from server */
			if (events[i].events & EPOLLIN) {
				mylog(L_DEBUG, "receiving from server");
				while (my->s2c_wactive) {
					res = connection_recvv_nb(my->server_conn, my->s2c_buf, DATA_BUFMAX);
					if (res < 0) { 
						if (errno == EAGAIN || errno == EINTR) {
							mylog(L_WARNING, "nonblock receive data from server failed, try again");
							break; //non-block
						} else {
							/* generate error message to client */
							mylog(L_WARNING, "receive data from server failed, close connection");
							proxy_context_connection_failed(my);
							my->state = STATE_TERM;
							proxy_context_put_termqueue(my);
							return 0;
						}
					}

					if (res == 0) {
						/* server close */
						mylog(L_WARNING, "server close");
						break;
					}

					mylog(L_DEBUG, "recv %d from server", res);
					if (my->s2c_buf->bufsize == my->s2c_buf->max) {
						my->s2c_wactive = 0;
						break;
					}
				}
			} 

			/* send to server */
			if (events[i].events & EPOLLOUT) {
				mylog(L_DEBUG, "sending to server");
				while (buffer_nbytes(my->c2s_buf) > 0) {
					res = connection_sendv_nb(my->server_conn, my->c2s_buf, DATA_SENDSIZE);
					if (res < 0) { 
						if (errno == EAGAIN || errno == EINTR) {
							mylog(L_WARNING, "nonblock send data to server failed, try again");
							break; //non-block
						} else {
							/* generate error message to client */
							mylog(L_DEBUG, "send data to server failed, close connection");
							proxy_context_connection_failed(my);
							my->state = STATE_TERM;
							proxy_context_put_termqueue(my);
							return 0;
						}
					}
					if (res == 0) {
						break;
					}
					mylog(L_DEBUG, "sent %d to server", res);
					if (my->c2s_buf->bufsize < my->c2s_buf->max) {
						my->c2s_wactive = 1;
					}
				}
			} 
		}

		/* client conn */
		if (events[i].data.u32 == 1) {
			/* recv from client */
			if (events[i].events & EPOLLIN) {
				mylog(L_DEBUG, "receiving from client");
				while (my->c2s_wactive) {
					res = connection_recvv_nb(my->client_conn, my->c2s_buf, DATA_BUFSIZE);
					if (res < 0) { 
						if (errno == EAGAIN || errno == EINTR) {
							mylog(L_WARNING, "nonblock receive data from client failed, try again");
							break; //non-block
						} else {
							mylog(L_WARNING, "receive data from client failed, close connection");
							my->state = STATE_TERM;
							proxy_context_put_termqueue(my);
							return 0;
						}
					}
					if (res == 0) {
						/* client close */
						mylog(L_DEBUG, "client is closed, close all connection, STATE_CLOSE, %p", my);
						my->state = STATE_CLOSE;
						/* remove my from io event pool */
						proxy_context_put_runqueue(my);
						return 0;
					}

					mylog(L_DEBUG, "recv %d from client", res);
					if (my->c2s_buf->bufsize == my->c2s_buf->max) {
						my->c2s_wactive = 0;
						break;
					}
				}

			}

			/* send to client */
			if (events[i].events & EPOLLOUT) {
				mylog(L_DEBUG, "sending to client");
				while (buffer_nbytes(my->s2c_buf) > 0) {
					res = connection_sendv_nb(my->client_conn, my->s2c_buf, DATA_SENDSIZE);
					if (res < 0) { 
						if (errno == EAGAIN || errno == EINTR) {
							mylog(L_WARNING, "nonblock send data to client failed, try again");
							break; //non-block
						} else {
							// client error
							mylog(L_WARNING, "send data from client failed, close connection");
							my->state = STATE_TERM;
							proxy_context_put_termqueue(my);
							return 0;
						}
					}
					if (res == 0) {
						break;
					}
					mylog(L_DEBUG, "send %d to client", res);
					if (my->s2c_buf->bufsize < my->s2c_buf->max) {
						my->s2c_wactive = 1;
					}
				}
			}
		}
	}
putfd:
	/* arm timer */	
	proxy_context_settimer(my->timer, &my->server_r_timeout_tv);

	mylog(L_DEBUG, "outoff iowait %p", my);
	proxy_context_put_epollfd(my);

	return 0;
}


int proxy_context_driver_close(proxy_context_t *my)
{
	mylog(L_DEBUG, "begin driver close, %p", my);
	proxy_context_retrieve_epollfd(my);
	return proxy_context_delete(my);
}

int proxy_context_driver_rejectclient(proxy_context_t *my)
{
	mylog(L_ERR, "begin driver rejectclient, %p", my);

	proxy_context_generate_message(my->s2c_buf, HTTP_503);

	my->state = STATE_IOWAIT;

	proxy_context_settimer(my->timer, &my->client_s_timeout_tv);
	proxy_context_put_epollfd(my);

	return 0;
}

int proxy_context_driver_dnsprobe(proxy_context_t *my)
{
	int ret = 0;
	char host[HOSTNAMESIZE], host_org[HOSTNAMESIZE];
	char *server_ip, *tmp;
	struct hostent *h;
	server_info_t sinfo;

	mylog(L_DEBUG, "context driver dnsprobe");

	memcpy(host, my->http_header.host.ptr, my->http_header.host.size);
	host[my->http_header.host.size] = '\0';
	memcpy(host_org, my->http_header.host.ptr, my->http_header.host.size);
	host_org[my->http_header.host.size] = '\0';

	tmp = strchr(host, ':');
	if (tmp) {
		*tmp = '\0';
	}

	h = gethostbyname(host);
	if (h) {
		server_ip = inet_ntoa(*((struct in_addr *)(h->h_addr_list[0])));
		strncpy(sinfo.hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
		sinfo.hostname[my->http_header.host.size] = '\0';
		sinfo.saddr.sin_family = AF_INET;
		sinfo.saddr.sin_port = htons(my->server_port);
		inet_pton(AF_INET, server_ip, &sinfo.saddr.sin_addr); 
		server_state_set_addr(host_org, &sinfo.saddr);
		server_state_set_state(host_org, SS_UNKNOWN);
	}

	my->state = STATE_TERM;
	proxy_context_put_runqueue(my);

	return ret;
}

int proxy_context_driver_connprobe(proxy_context_t *my)
{
	int ret, i;
	struct timeval tv;
	struct epoll_event events[2];
	int nfds;

	mylog(L_DEBUG, "context driver connprobe");

	nfds = epoll_wait(my->epoll_context, events, 2, 0);

	/* disarm timer */
	tv.tv_sec = 0;
	tv.tv_usec = 0;
	proxy_context_settimer(my->timer, &tv);

	if (nfds < 0) {
		mylog(L_WARNING, "driver connprobe epoll_wait return %d", nfds);
		proxy_context_put_epollfd(my);
	}

	if (nfds == 0) {
		ret = 1;
	}

	/* if fd event happend ignore timeout */
	for (i = 0; i < nfds; i++) {
		if (events[i].data.u32 == 2) {
			ret = 0;
		} else {
			ret = 1;
			break;
		}
	}

	if (ret == 0) {
		/* timeout, close server conn immediately */
		if (my->server_conn && connection_close_nb(my->server_conn)) {
			mylog(L_ERR, "close server connectin failed");
		}
		my->server_conn = NULL;

		proxy_context_generate_message(my->s2c_buf, HTTP_504);

		my->state = STATE_IOWAIT;
		proxy_context_settimer(my->timer, &my->client_s_timeout_tv);

		proxy_context_put_epollfd(my);
	} else {
		/* connect probe */
		ret = connection_connect_nb(&my->server_conn, my->server_ip, my->server_port);
		if (ret == 0 || ret == EISCONN) {
			//mylog(L_ERR, "err is %d, change state into IOWAIT", err);
			mylog(L_DEBUG, "err is %d, change state into STATE_TERM", ret);
			my->state = STATE_TERM;
			if (my->set_dict) {
				char hostname[HOSTNAMESIZE];
				strncpy(hostname, (void *)my->http_header.host.ptr, my->http_header.host.size);
				hostname[my->http_header.host.size] = '\0';
				server_state_set_state(hostname, SS_OK);
			}
			proxy_context_put_termqueue(my);
		} else if (ret != EINPROGRESS && ret != EALREADY) {
			mylog(L_DEBUG, "some error occured, error is %d(%s) reject client", ret, strerror(ret));
			my->state = STATE_TERM;
			proxy_context_put_termqueue(my);
		} else {
			mylog(L_DEBUG, "err is EINPROGRESS, will retry connect server");
			proxy_context_settimer(my->timer, &my->server_connect_timeout_tv);
			proxy_context_put_epollfd(my); //EINPROGRESS
		}
	}

	return 0;
}

int proxy_context_driver_error(proxy_context_t *my)
{
	int ret=0;

	mylog(L_DEBUG, "context driver error occurred");
	my->state = STATE_TERM;
	proxy_context_put_termqueue(my);

	return ret;
}

int proxy_context_driver_term(proxy_context_t *my)
{
	//	int ret;
	//mylog(L_ERR, "context driver terminate");
	mylog(L_DEBUG, "context driver terminate, %p", my);

	proxy_context_retrieve_epollfd(my);
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
		case STATE_PARSEHEADER:
			ret = proxy_context_driver_parseheader(my);
			break;
		case STATE_CONNECTSERVER:
			ret = proxy_context_driver_connectserver(my);
			break;
		case STATE_IOWAIT:
			ret = proxy_context_driver_iowait(my);
			break;
		case STATE_REJECTCLIENT:
			ret = proxy_context_driver_rejectclient(my);
			break;
		case STATE_CLOSE:
			ret = proxy_context_driver_close(my);
			break;
		case STATE_DNS_PROBE:
			ret = proxy_context_driver_dnsprobe(my);
			break;
		case STATE_CONN_PROBE:
			ret = proxy_context_driver_connprobe(my);
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
	mylog(L_DEBUG, "leaving drive, %p", my);
	return ret;
}

cJSON *proxy_context_serialize(proxy_context_t *my)
{
	cJSON *result;

	result = cJSON_CreateObject();

	cJSON_AddNumberToObject(result, "State", my->state);
	cJSON_AddNumberToObject(result, "ListenSd", my->listen_sd);
	cJSON_AddNumberToObject(result, "EpollFd", my->epoll_context);
	cJSON_AddItemToObject(result, "ServerConnection", connection_serialize(my->server_conn));
	cJSON_AddItemToObject(result, "ClientConnection", connection_serialize(my->client_conn));

	return result;
}

