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

#include "ds_bufferlist.h"
#include "util_conn_tcp.h"
#include "util_log.h"


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

static void conn_tcp_update_time(struct timeval *tv)
{
	gettimeofday(tv, NULL);
}

static conn_tcp_t * conn_tcp_init()
{
	conn_tcp_t *c;

	c = calloc(1, sizeof(conn_tcp_t));
	if (c == NULL) {
		return NULL;
	}

	conn_tcp_set_timeout(c, 200);
	conn_tcp_update_time(&c->build_tv);
	
	return c;
}

/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int conn_tcp_accept_nb(conn_tcp_t **conn, int listen_sd)
{
	conn_tcp_t tmp;
	int saveflg;

	memset(&tmp, 0, sizeof(tmp));
	conn_tcp_update_time(&tmp.build_tv);

	saveflg = fcntl(listen_sd, F_GETFL);
	if ((saveflg & O_NONBLOCK) == 0) {
		fcntl(listen_sd, F_SETFL, saveflg | O_NONBLOCK);
	}

	tmp.sd = accept(listen_sd, &tmp.peer_addr, &tmp.peer_addrlen);
	if (tmp.sd<0) {
		return errno;
	}
	conn_tcp_update_time(&tmp.first_c_tv);
	int val=1;
	if (setsockopt(tmp.sd, IPPROTO_TCP, TCP_CORK, &val, sizeof(val))) {
		mylog(L_WARNING, "setsockopt(sd, TCP_CORK) failed: %m");
	}
	val=65536*8;
	if (setsockopt(tmp.sd, SOL_SOCKET, SO_RCVBUF, &val, sizeof(val))) {
		mylog(L_WARNING, "setsockopt(sd, SO_RCVBUF) failed: %m");
	}

	inet_ntop(AF_INET, &((struct sockaddr_in*)&(tmp.peer_addr))->sin_addr, tmp.peer_host, 40);
	tmp.peer_port = ntohs(((struct sockaddr_in*)&(tmp.peer_addr))->sin_port);

	tmp.local_addrlen = sizeof(struct sockaddr);
	getsockname(tmp.sd, &tmp.local_addr, &tmp.local_addrlen);

	*conn = conn_tcp_init();
	if (*conn== NULL) {
		close(tmp.sd);
		return ENOMEM;
	}
	memcpy(*conn, &tmp, sizeof(tmp));

	return 0;
}

/*
 * Connect to a server and create the connection struct.
 */
int conn_tcp_connect_nb(conn_tcp_t **conn, char *peer_host, uint16_t peer_port)
{
	int sd, ret;
	struct sockaddr_in sa;
	int saveflg;
	conn_tcp_t *c = *conn;

	if (c == NULL) {
		sd = socket(AF_INET, SOCK_STREAM, 0);
		if (sd < 0) {
			return errno;
		}
		
		saveflg = fcntl(sd, F_GETFL);
		if ((saveflg & O_NONBLOCK) == 0) {
			fcntl(sd, F_SETFL, saveflg | O_NONBLOCK);
		}

		*conn = conn_tcp_init();
		if (*conn == NULL) {
			close(sd);
			return ENOMEM;
		}
		
		c = *conn;
		c->sd = sd;

		strcpy(c->peer_host, peer_host);

		sa.sin_family = AF_INET;
		sa.sin_port = htons(peer_port);
		inet_pton(AF_INET, peer_host, &sa.sin_addr);
		memcpy(&c->peer_addr, &sa, sizeof(struct sockaddr));
		c->peer_addrlen = sizeof(struct sockaddr);
	}

	ret = connect(c->sd, &c->peer_addr, c->peer_addrlen);
	if (ret<0) {
		return errno;
	}
	return 0;
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
			conn_tcp_update_time(&conn->first_r_tv);
		}
		conn_tcp_update_time(&conn->last_r_tv);
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
			conn_tcp_update_time(&conn->first_s_tv);
		}
		conn_tcp_update_time(&conn->last_s_tv);
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
	char ip4str[16];
	struct sockaddr_in *si;

	si = (struct sockaddr_in *)addr;

	inet_ntop(AF_INET, &si->sin_addr, ip4str, 16);

	result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "address", ip4str);
	cJSON_AddNumberToObject(result, "port", ntohs(si->sin_port));

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
	cJSON_AddItemToObject(result, "Peer", sockaddr_in_json(&conn->peer_addr));
	cJSON_AddItemToObject(result, "Local", sockaddr_in_json(&conn->local_addr));
	cJSON_AddNumberToObject(result, "Timeout", timeval_sec(&conn->connecttimeo));
	cJSON_AddNumberToObject(result, "BytesIn", (double)(conn->rcount));
	cJSON_AddNumberToObject(result, "BytesOut", (double)(conn->scount));
	cJSON_AddNumberToObject(result, "Descriptor", conn->sd);
	// TODO: Add more.
	//
	return result;
}


