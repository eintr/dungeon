/** \file util_conn_tcp.h
*/

#ifndef CONN_TCP_H
#define CONN_TCP_H

/** \cond 0 */
#include <sys/socket.h>
#include <stdint.h>
#include <netdb.h>
#include "cJSON.h"
/** \endcond */

#include "ds_bufferlist.h"

#define DATA_BUFSIZE 4096
#define DATA_BUFMAX 4096


#define DEFAULT_IOVS 8

/** Timeout desctiption of a connection */
typedef struct timeout_msec_st {
	uint32_t recv, send;
	uint32_t connect;
	uint32_t accept;
} timeout_msec_t;

/** The TCP listen struct */
typedef struct listen_tcp_st {
	struct sockaddr_storage local_addr;
	socklen_t local_addrlen;
	struct timeval build_tv;
	struct timeval accepttimeo;
	int64_t accept_count;
	int sd;
} listen_tcp_t;

/** The TCP connection struct */
typedef struct conn_tcp_st {
	char peer_host[40];
	uint16_t peer_port;		// In host byte-order
	struct sockaddr_storage peer_addr;
	struct sockaddr_storage local_addr; // used for debug
	socklen_t peer_addrlen, local_addrlen;
	struct timeval recvtimeo, sendtimeo;
	struct timeval connecttimeo;
	struct timeval conn_tv;
	struct timeval first_r_tv, first_s_tv;
	struct timeval last_r_tv, last_s_tv;
	int64_t rcount, scount;
	int sd;
} conn_tcp_t;

/** Create listen socket, bind, and prepare for accept() */
listen_tcp_t *conn_tcp_listen_create(struct addrinfo *local, timeout_msec_t*);

/** Create listen socket, bind, and prepare for accept() */
int conn_tcp_listen_destroy(listen_tcp_t*);

/**
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int conn_tcp_accept_nb(conn_tcp_t**, listen_tcp_t *, timeout_msec_t*);

/**
 * Connect to a server and create the connection struct.
 */
int conn_tcp_connect_nb(conn_tcp_t**, char *peer_host, uint16_t peer_port, timeout_msec_t *timeo);

/** Receive/send data from/to a connection struct. Nonblocked. */
ssize_t conn_tcp_recv_nb(conn_tcp_t*, void *buf, size_t size);
ssize_t conn_tcp_send_nb(conn_tcp_t*, const void *buf, size_t size);


/** Send/receivce data vector to/from a connection */
ssize_t conn_tcp_sendv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);
ssize_t conn_tcp_recvv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);

/** Flush and close a connection */
int conn_tcp_close_nb(conn_tcp_t *);

/**  */
cJSON *conn_tcp_serialize(conn_tcp_t *);

#endif

