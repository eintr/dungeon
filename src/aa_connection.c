#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h>

#include "aa_connection.h"

static void connection_set_timeout(connection_t *conn, uint32_t timeout)
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

static void connection_update_time(struct timeval *tv)
{
	gettimeofday(tv, NULL);
}

static connection_t * connection_init(int sd)
{
	connection_t *c;
	int ret;

	c = calloc(1, sizeof(connection_t));
	if (c == NULL) {
		return NULL;
	}
/*
	c->peer_addrlen = sizeof(struct sockaddr);
	ret = getpeername(sd, (struct sockaddr *)&c->peer_addr, &c->peer_addrlen);
	if (ret != 0) {
		free(c);
		return NULL;
	}

	c->peer_host = inet_ntoa(c->peer_addr.sin_addr);
	c->peer_port = ntohs(c->peer_addr.sin_port);

	c->local_addrlen = sizeof(struct sockaddr);
	ret = getsockname(sd, (struct sockaddr *)&c->local_addr, &c->local_addrlen);
	if (ret != 0) {
		free(c);
		return NULL;
	}
	c->sd = sd;
	c->rcount = c->scount = 0;
*/

	connection_set_timeout(c, 200);
	connection_update_time(&c->build_tv);

	/*
	connection_update_time(&c->first_c_tv);

	c->build_tv.tv_sec = 0;
	c->build_tv.tv_usec = 0;
	c->first_c_tv.tv_sec = 0;
	c->first_c_tv.tv_usec = 0;
	c->first_r_tv.tv_sec = 0;
	c->first_r_tv.tv_usec = 0;
	c->first_s_tv.tv_sec = 0;
	c->first_s_tv.tv_usec = 0;
	c->last_r_tv.tv_sec = 0;
	c->last_r_tv.tv_usec = 0;
	c->last_s_tv.tv_sec = 0;
	c->last_s_tv.tv_usec = 0;
*/
}

/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int connection_accept_nb(connection_t **conn, int listen_sd)
{
	int sd;
	connection_t tmp;
	struct sockaddr_in sa;
	socket_t socklen;
	int ret;
	long saveflg;

	memset(&tmp, 0, sizeof(tmp));
	connection_update_time(&tmp.build_tv);

	saveflg = fcntl(listen_sd, F_GETFL);
	if (saveflg&O_NONBLOCK==0) {
		fcntl(listen_sd, F_GETFL|O_NONBLOCK);
	}

	tmp.sd = accept(listen_sd, &tmp.peer_addr, &tmp.peer_addrlen);
	if (tmp.sd<0) {
		return errno;
	}
	connection_update_time(&tmp.first_c_tv);

	inet_ntop(AF_INET, &((struct sockaddr_in*)&(tmp.peer_addr))->sin_addr, &tmp.peer_host, 40);
	tmp.peer_port = ntohs(&((struct sockaddr_in*)&(tmp.peer_addr))->sin_port);

	tmp.local_addrlen = sizeof(struct sockaddr);
	getsockname(tmp.sd, tmp.local_addr, &tmp.local_addrlen);

	*conn = malloc(sizeof(connection_t));
	if (*conn== NULL) {
		close(sd);
		return ENOMEM;
	}
	memcpy(*conn, &tmp, sizeof(tmp));

	return 0;
}

/*
 * Connect to a server and create the connection struct.
 */
int connection_connect_nb(connection_t **conn, char *peer_host, uint16_t peer_port)
{
	int sd, ret, saveerr;
	struct sockaddr_in sa;

	if (*conn==NULL) {
		sd = socket(AF_INET, SOCK_STREAM, 0);
		if (sd < 0) {
			return errno;
		}

		*conn = calloc(1, sizeof(connection_t));
		if (*conn== NULL) {
			close(sd);
			return ENOMEM;
		}

		*conn->sd = sd;

		strcpy(*conn->peer_host, peer_host);

		sa.sin_family = AF_INET;
		sa.sin_port = htons(peer_port);
		inet_pton(AF_INET, peer_host, &sa.sin_addr);
		memcpy(&(*conn)->peer_addr, &sa, sizeof(sa));
		*conn->peer_addrlen = sizeof(sa);
	}

	ret = connect(*conn->sd, &*conn->peer_addr, *conn->peer_addrlen);
	if (ret<0) {
		return errno;
	}
	return 0;
}

/*
 * Read/write data from/to a connection struct.
 * Nonblocked.
 */
ssize_t connection_recv_nb(connection_t *conn, void *buf, size_t size)
{
	int res = 0;
	res = recv(conn->sd, buf, size, MSG_DONTWAIT);
	if (res > 0) {
		if (conn->first_r_tv.sec == 0) {
			connection_update_time(&conn->first_r_tv);
		}
		connection_update_time(&conn->last_r_tv);
		conn->rcount+=res;
	}
	return res;
}

ssize_t connection_send_nb(connection_t *conn, const void *buf, size_t size)
{
	int res = 0;

	res = send(conn->sd, buf, size, MSG_DONTWAIT);
	if (res > 0) {
		if (conn->first_s_tv.sec == 0) {
			connection_update_time(&conn->first_s_tv);
		}
		connection_update_time(&conn->last_s_tv);
		conn->scount+=res;
	}
	return res;
}

/*
 * Close a connection.
 */
int connection_close_nb(connection_t *conn)
{
	close(conn->sd);
	free(conn);
	return 0;
}

ssize_t connection_sendv_nb(connection_t *conn, buffer_list_t *bl, size_t size)
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
		bn = (buffer_node_t *) buffet_get_data(buf);
		if (bn) {
			iovs[i].iov_base = bn->start;
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

	res = writev(conn->fd, iovs, i);

	if (res < 0) {
		//log error;
		return res;
	}

	bytes = res;

	bn = NULL;

	while (bytes > 0) {
		buffer_get_head(bl->base, (void **)&bn);
		if (bytes > bn->size) {
			bytes -= bn->size;
			buffer_pop(bl);
		} else {
			buffer_move_head(bl, bytes);
			bytes = 0;
		}
	} 

	return res;
}



ssize_t connection_recvv_nb(connection_t *conn, buffer_list_t *bl, size_t size)
{
	char *buf;
	size_t s, total = 0;
	int res;
	int ret;

	buf = (char *) malloc(DATA_BUFSIZE);
	if (buf == NULL) {
		return -1;
	}

	while (size > 0) {
		if (size > DATA_BUFSIZE) {
			s = DATA_BUFSIZE;
		} else {
			s = size;
		}

		size -= s;

		res = connection_recv_nb(conn, buf, s);
		if (res < 0) {
			free(buf);
			if (errno == EAGAIN || errno == EINTR) {
				return total;
			} else {
				return -1;
			}
		}

		ret = buffer_write(bl, buf, res);
		if (ret < 0) {
			break;	
		}
		
		total += res;

		/* buffer list is full */
		if (bl->bufsize == bl->max) {
			break;
		}
	}

	free(buf);
	return total;
}

/* TODO: Move below 2 functions to correct place. */

static cJSON *sockaddr_in_json(struct sockaddr_in *addr)
{
	cJSON *result;
	char ip4str[16];

	inet_ntop(AF_INET, addr->sin_addr, ip4str, 16);

	result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "address", ip4str);
	cJSON_AddNumberToObject(result, "port", ntohs(addr->sini_port));

	return result;
}

static double timeval_sec(struct timeval *tv)
{
	return (double)tv->sec + ((double)tv->usec)/1000.0F/1000.0F;
}

/**************************************************/

cJSON *connection_serialize(connection_t *conn)
{
	cJSON *result;

	result = cJSON_CreateObject();
	cJSON_AddItemToObject(result, "peer", sockaddr_in_json(conn->peer_addr));
	cJSON_AddItemToObject(result, "local", sockaddr_in_json(conn->local_addr));
	cJSON_AddNumberToObject(result, "time_out", timeval_sec(conn->connecttimeo));
	// TODO: Add more.
	//
	return result;
}

#endif

