#include "aa_connection.h"

void connection_set_timeout(connection_t *conn, uint32_t timeout)
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

void connection_update_time(struct timeval *tv)
{
	gettimeofday(tv, NULL);
}

connection_t * connection_init(int sd)
{
	connection_t *c;
	int ret;

	c = (connection_t *) malloc(connection_t);
	if (c == NULL) {
		return NULL;
	}

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

	connection_set_timeout(c, 200);

	connection_update_time(&c->build_tv);
	connection_update_time(&c->first_c_tv);

	c->first_c_tv.tv_usec = 0;
	c->first_r_tv.tv_sec = 0;
	c->first_r_tv.tv_usec = 0;
	c->first_s_tv.tv_sec = 0;
	c->first_s_tv.tv_usec = 0;
	c->last_r_tv.tv_sec = 0;
	c->last_r_tv.tv_usec = 0;
	c->last_s_tv.tv_sec = 0;
	c->last_s_tv.tv_usec = 0;

}
/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int connection_accept_nb(connection_t **conn, int listen_sd)
{
	int sd;
	connection_t *c;
	struct sockaddr_in sa;
	socket_t socklen;
	int ret;

	sd = accpet(listen_sd, NULL, NULL);
	if (sd == -1) {
		return -1;
	}

	c = connection_init(sd);
	if (c == NULL) {
		return -1;
	}

	*conn = c;

	return 0;
}

/*
 * Connect to a server and create the connection struct.
 */
int connection_connect_nb(connection_t **conn, char *peer_host, uint16_t peer_port)
{
	int ret;
	int sd;
	struct sockaddr_in sa;
	connection_t *c;


	sd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (sd < 0) {
		return sd;
	}

	sa.sin_port = htons(peer_port);
	sa.sin_family = AF_INET;
	inet_aton(peer_host, &sa.sin_addr);

	ret = connect(sd, (struct sockaddr *)&sa, sizeof(sa));
	if (ret == -1) {
		if (errno == EISCONN) { 
			ret = 0;
		} else if (errno == EINPROGRESS) { //nonblock connect
			ret = 1;
		} else {
			return -1;
		}
	}

	c = connection_init(sd);
	if (c == NULL) {
		close(sd);
		return -2; //register server failed
	}

	*conn = c;

	return ret;
}

/*
 * Read/write data from/to a connection struct.
 * Nonblocked.
 */
ssize_t connection_recv_nb(connection_t *conn, void *buf, size_t size)
{
	int res = 0;
	res = read(conn->sd, buf, size);
	if (res > 0) {
		if (conn->first_r_tv.sec == 0) {
			connection_update_time(&conn->first_r_tv);
		}
		connection_update_time(&conn->last_r_tv);
		conn->rcount++;
	}
	return res;
}
ssize_t connection_send_nb(connection_t *conn, const void *buf, size_t size)
{
	int res = 0;

	res = write(conn->sd, buf, size);
	if (res > 0) {
		if (conn->first_s_tv.sec == 0) {
			connection_update_time(&conn->first_s_tv);
		}
		connection_update_time(&conn->last_s_tv);
		conn->scount++;
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

cJSON *connection_serialize(connection_t *conn);

#endif

