/** \file util_conn_tcp.c
*/


/** \cond 0 */
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/ioctl.h>
/** \endcond */

#include "util_conn_tcp.h"
#include "util_log.h"

static void update_timeval(struct timeval *tv)
{
	gettimeofday(tv, NULL);
}

void msec_2_timeval(struct timeval *tv, uint32_t ms)
{
	tv->tv_sec = ms/1000;
	tv->tv_usec = (ms%1000)*1000;
}

listen_tcp_t *conn_tcp_listen_create(struct addrinfo *local, timeout_msec_t *timeo)
{
	listen_tcp_t *m;
	int sdflags;

	m = malloc(sizeof(*m));
	if (m==NULL) {
		return NULL;
	}
	m->sd = socket(local->ai_family, local->ai_socktype, local->ai_protocol);
	if (m->sd < 0) {
		free(m);
		return NULL;
	}
	sdflags = fcntl(m->sd, F_GETFL);
	fcntl(m->sd, F_SETFL, sdflags|O_NONBLOCK);
	if (bind(m->sd, local->ai_addr, local->ai_addrlen)<0) {
		close(m->sd);
		free(m);
		return NULL;
	}
	if (listen(m->sd, 100)<0) {
		close(m->sd);
		free(m);
		return NULL;
	}
	update_timeval(&m->build_tv);
	if (timeo) {
		msec_2_timeval(&m->accepttimeo, timeo->accept);
	}
	m->accept_count = 0;
	
	return m;
}

int conn_tcp_listen_destroy(listen_tcp_t *l)
{
	close(l->sd);
	free(l);
	return 0;
}
/*
static void conn_tcp_set_timeout(conn_tcp_t *conn, uint32_t timeout)
{
	uint32_t sec, msec;	
	sec = timeout / 1000;
	msec = timeout % 1000;
	conn->connecttimeo.tv_sec = sec;
	conn->connecttimeo.tv_usec = msec * 1000;
	conn->recvtimeo.tv_sec = sec;
	conn->recvtimeo.tv_usec = msec * 1000;
	conn->sendtimeo.tv_sec = sec;
	conn->sendtimeo.tv_usec = msec * 1000;
}

static conn_tcp_t * conn_tcp_init()
{
	conn_tcp_t *c;

	c = calloc(1, sizeof(conn_tcp_t));
	if (c == NULL) {
		return NULL;
	}

	conn_tcp_set_timeout(c, 200);
	
	return c;
}
*/

/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int conn_tcp_accept_nb(conn_tcp_t **conn, listen_tcp_t *l, timeout_msec_t *timeo)
{
	//conn_tcp_t tmp;
	int save;

	*conn = calloc(sizeof(conn_tcp_t), 1);
	if (*conn == NULL) {
		return errno;
	}

	(*conn)->peer_addrlen = sizeof((*conn)->peer_addr);

	(*conn)->sd = accept(l->sd, (void*)&((*conn)->peer_addr), &((*conn)->peer_addrlen));
	if ((*conn)->sd<0) {
		free(*conn);
		*conn=NULL;
		return errno;
	}
	save = fcntl((*conn)->sd, F_GETFL);
	if ((save & O_NONBLOCK) == 0) {
		fcntl((*conn)->sd, F_SETFL, save|O_NONBLOCK);
	}

	memcpy(&((*conn)->local_addr), &l->local_addr, (*conn)->local_addrlen);
	(*conn)->local_addrlen = l->local_addrlen;

	if (timeo) {
		msec_2_timeval(&((*conn)->connecttimeo), timeo->connect);
		msec_2_timeval(&((*conn)->recvtimeo), timeo->recv);
		msec_2_timeval(&((*conn)->sendtimeo), timeo->send);
	}

	int val=1;
	if (setsockopt((*conn)->sd, IPPROTO_TCP, TCP_CORK, &val, sizeof(val))) {
		mylog(L_WARNING, "setsockopt(sd, TCP_CORK) failed: %m");
	}
	val=65536*8;
	if (setsockopt((*conn)->sd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val))) {
		mylog(L_WARNING, "setsockopt(sd, SO_RCVBUF) failed: %m");
	}

/*	inet_ntop(AF_INET, &((struct sockaddr_in*)&(tmp.peer_addr))->sin_addr, tmp.peer_host, 40);
	tmp.peer_port = ntohs(((struct sockaddr_in*)&(tmp.peer_addr))->sin_port);
*/
	return 0;
}

/*
 * Connect to a server and create the connection struct.
 */
int conn_tcp_connect_nb(conn_tcp_t **conn, struct addrinfo *peer, timeout_msec_t *timeo)
{
	int sd, ret;
	int saveflg;
	conn_tcp_t *c = *conn;

	if (c == NULL) {
		sd = socket(peer->ai_family, peer->ai_socktype, peer->ai_protocol);
		if (sd < 0) {
			return errno;
		}
		
		saveflg = fcntl(sd, F_GETFL);
		if ((saveflg & O_NONBLOCK) == 0) {
			fcntl(sd, F_SETFL, saveflg | O_NONBLOCK);
		}

		*conn = calloc(sizeof(*conn), 1);
		if (*conn == NULL) {
			close(sd);
			return ENOMEM;
		}

		if (timeo!=NULL) {
			msec_2_timeval(&(*conn)->connecttimeo, timeo->connect);
			msec_2_timeval(&(*conn)->recvtimeo, timeo->recv);
			msec_2_timeval(&(*conn)->sendtimeo, timeo->send);
		} else {
			msec_2_timeval(&(*conn)->connecttimeo, -1);
			msec_2_timeval(&(*conn)->recvtimeo, -1);
			msec_2_timeval(&(*conn)->sendtimeo, -1);
		}

		c = *conn;
		c->sd = sd;

		memcpy(&c->peer_addr, peer->ai_addr, peer->ai_addrlen);
		c->peer_addrlen = peer->ai_addrlen;
	}

	ret = connect(c->sd, (void*)&c->peer_addr, c->peer_addrlen);
	if (ret<0) {
		return errno;
	}
	return 0;
}

size_t conn_tcp_suggest_bufsize(conn_tcp_t *c)
{
	int err, value;

	err = ioctl(c->sd, FIONREAD, &value);
	if (err) {
		return 2048;
	}
	return value;
}

/*
 * Read/write data from/to a connection struct.
 * Nonblocked.
 */
ssize_t conn_tcp_recv_nb(conn_tcp_t *conn, void *buf, size_t size)
{
	int res = 0;
	res = recv(conn->sd, buf, size, MSG_DONTWAIT);
	if (res > 0) {
		if (conn->first_r_tv.tv_sec == 0) {
			update_timeval(&conn->first_r_tv);
		}
		update_timeval(&conn->last_r_tv);
		conn->rcount+=res;
		return res;
	} else if (res == 0){
		return res;
	} else {
		return -errno;
	}
}

ssize_t conn_tcp_send_nb(conn_tcp_t *conn, const void *buf, size_t size)
{
	int res = 0;

	res = send(conn->sd, buf, size, MSG_DONTWAIT);
	if (res > 0) {
		if (conn->first_s_tv.tv_sec == 0) {
			update_timeval(&conn->first_s_tv);
		}
		update_timeval(&conn->last_s_tv);
		conn->scount+=res;
	}
	return res;
}

/*
 * Close a connection.
 */
int conn_tcp_close_nb(conn_tcp_t *conn)
{
	if (conn->sd != -1) {
		close(conn->sd);
		conn->sd = -1;
	}
	free(conn);
	return 0;
}

ssize_t conn_tcp_sendv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size)
{
	struct iovec iovs[DEFAULT_IOVS];
	void *buf;
	buffer_node_t *bn;
	int i;
	size_t bytes;
	ssize_t res;

	i = 0;
	bytes = size;

	buf = buffer_get_head(bl);

	while (buf && (bytes > 0) && (i < DEFAULT_IOVS)) {
		bn = (buffer_node_t *) buffer_get_data(buf);
		if (bn) {
			iovs[i].iov_base = bn->pos;
			if (bytes > bn->size) {
				iovs[i].iov_len = bn->size;
				bytes -= bn->size;
			} else {
				iovs[i].iov_len = bytes;
				bytes = 0;
			}
			buf = buffer_get_next(bl, buf);
			i++;
		}
	}

	res = writev(conn->sd, iovs, i);

	if (res < 0) {
		//mylog(L_ERR, "writev failed");
		return res;
	}
	/*
	if (res == 0) {
		mylog(L_INFO, "writev write %d data", res);
	}
*/
	bytes = res;

	bn = NULL;
	buf = NULL;

	while (bytes > 0) {
		buf = buffer_get_head(bl);
		if (buf) {
			bn = (buffer_node_t *) buffer_get_data(buf);
			if (bytes >= bn->size) {
				bytes -= bn->size;
				//mylog(L_ERR, "bl->bufsize is %d, bl->base->nr_nodes is %d", bl->bufsize, bl->base->nr_nodes);
				buffer_pop(bl);
			} else {
				buffer_move_head(bl, bytes);
				bytes = 0;
			}
		} else {
			/* should never happend */
			//mylog(L_ERR, "pop node from buffer list error");
			break;
		}
	} 

	return res;
}

ssize_t conn_tcp_recvv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size)
{
	char *buf;
	size_t s, total = 0;
	int res;
	int ret;

	buf = (char *) malloc(DATA_BUFSIZE);
	if (buf == NULL) {
		return -EINVAL;
	}

	while (size > 0) {
		if (size > DATA_BUFSIZE) {
			s = DATA_BUFSIZE;
		} else {
			s = size;
		}

		size -= s;

		res = conn_tcp_recv_nb(conn, buf, s);
		//mylog(L_DEBUG, "recv result is %d", res);

		if (res <= 0) {
			free(buf);
			if ((errno == EAGAIN || errno == EINTR) && total > 0) {
				//mylog(L_DEBUG, "res < 0 , return total");
				return total;
			} else {
				//mylog(L_DEBUG, "res <= 0, return res or -errno");
				if (res == 0) {
					return res;
				}
				return -errno;
			}
		} 

		ret = buffer_write(bl, buf, res);
		if (ret < 0) {
			break;	
		}

		total += res;

		if (bl->bufsize == bl->max) {
			//mylog(L_DEBUG, "buffer list is full.");
			break;
		}
	}

	free(buf);
	return total;
}

/* TODO: Move below 2 functions to correct place. */

static cJSON *sockaddr_in_json(struct sockaddr *addr)
{
	cJSON *result;
	char ipstr[40];
	struct sockaddr_in *si;
	struct sockaddr_in6 *si6;

	result = cJSON_CreateObject();

	switch (addr->sa_family) {
	case AF_INET:
		si = (struct sockaddr_in *)addr;
		inet_ntop(AF_INET, &si->sin_addr, ipstr, 40);
		cJSON_AddStringToObject(result, "address", ipstr);
		cJSON_AddNumberToObject(result, "port", ntohs(si->sin_port));
		break;
	case AF_INET6:
		si6 = (struct sockaddr_in6 *)addr;
		inet_ntop(AF_INET6, &si6->sin6_addr, ipstr, 40);
		cJSON_AddStringToObject(result, "address", ipstr);
		cJSON_AddNumberToObject(result, "port", ntohs(si6->sin6_port));
		break;
	default:
		cJSON_AddStringToObject(result, "address", "UNKNOWN");
		cJSON_AddNumberToObject(result, "port", 0);
		break;
	} 

	return result;
}

static double timeval_sec(struct timeval *tv)
{
	return (double)tv->tv_sec + ((double)tv->tv_usec)/1000.0F/1000.0F;
}

/**************************************************/

cJSON *conn_tcp_serialize(conn_tcp_t *conn)
{
	cJSON *result;

	result = cJSON_CreateObject();
	cJSON_AddItemToObject(result, "Peer", sockaddr_in_json((void*)&conn->peer_addr));
	cJSON_AddItemToObject(result, "Local", sockaddr_in_json((void*)&conn->local_addr));
	cJSON_AddNumberToObject(result, "Timeout", timeval_sec(&conn->connecttimeo));
	cJSON_AddNumberToObject(result, "BytesIn", (double)(conn->rcount));
	cJSON_AddNumberToObject(result, "BytesOut", (double)(conn->scount));
	cJSON_AddNumberToObject(result, "Descriptor", conn->sd);
	// TODO: Add more.
	//
	return result;
}


