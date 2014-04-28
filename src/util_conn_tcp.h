#ifndef CONN_TCP_H
#define CONN_TCP_H

#include <sys/socket.h>
#include <stdint.h>

#include "cJSON.h"
#include "ds_bufferlist.h"

#define DATA_BUFSIZE 4096
#define DATA_BUFMAX 4096


#define DEFAULT_IOVS 8

/* The connection struct */
typedef struct conn_tcp_st {
	char peer_host[40];
	uint16_t peer_port;		// In host byte-order
	struct sockaddr peer_addr;
	struct sockaddr local_addr; // used for debug
	socklen_t peer_addrlen, local_addrlen;
	struct timeval connecttimeo, recvtimeo, sendtimeo;
	struct timeval build_tv, first_c_tv;
	struct timeval first_r_tv, first_s_tv;
	struct timeval last_r_tv, last_s_tv;
	int64_t rcount, scount;
	int sd;
} conn_tcp_t;

/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int conn_tcp_accept_nb(conn_tcp_t**, int listen_sd);

/*
 * Connect to a server and create the connection struct.
 */
int conn_tcp_connect_nb(conn_tcp_t**, char *peer_host, uint16_t peer_port);

/*
 * Receive/send data from/to a connection struct.
 * Nonblocked.
 */
ssize_t conn_tcp_recv_nb(conn_tcp_t*, void *buf, size_t size);
ssize_t conn_tcp_send_nb(conn_tcp_t*, const void *buf, size_t size);


/*
 * Send/receivce data vector to/from a connection.
 */
ssize_t conn_tcp_sendv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);
ssize_t conn_tcp_recvv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);
/*
 * Flush and close a connection.
 */
int conn_tcp_close_nb(conn_tcp_t *);
cJSON *conn_tcp_serialize(conn_tcp_t *);

#endif

