#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>
#include <sys/socket.h>
#include <stdint.h>

#include "cJSON.h"

#include "cJSON.h"

#define DATA_BUFSIZE 1024
#define DATA_BUFMAX 4096


#define DEFAULT_IOVS 8

/* The connection struct */
typedef struct connection_st {
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
} connection_t;

/*
 * Accept a connection from the client and create the connection struct.
 * Nonblocked.
 */
int connection_accept_nb(connection_t**, int listen_sd);

/*
 * Connect to a server and create the connection struct.
 */
int connection_connect_nb(connection_t**, char *peer_host, uint16_t peer_port);

/*
 * Receive/send data from/to a connection struct.
 * Nonblocked.
 */
ssize_t connection_recv_nb(connection_t*, void *buf, size_t size);
ssize_t connection_send_nb(connection_t*, const void *buf, size_t size);

/*
 * Flush and close a connection.
 */
int connection_close_nb(connection_t *);

cJSON *connection_serialize(connection_t *);

#endif

