#ifndef UTIL_CONN_UDP_H
#define UTIL_CONN_UDP_H

#include <sys/socket.h>
#include <stdint.h>

#include "cJSON.h"
#include "ds_bufferlist.h"
#include "util_socket_udp.h"

#define DATA_BUFSIZE 4096
#define DATA_BUFMAX 4096


#define DEFAULT_IOVS 8

typedef struct dgram_st {
	struct sockaddr peer;
};

/* The soket wrapper */
typedef struct socket_udp_st {
    struct sockaddr local_addr; // used for debug
    socklen_t local_addrlen;
    struct timeval recvtimeo, sendtimeo;
    struct timeval build_tv;
    struct timeval first_r_tv, first_s_tv;
    struct timeval last_r_tv, last_s_tv;
    int64_t rcount_packet, scount_packet;
    int64_t rcount_byte, scount_byte;
	int sd;
	int event_fd;
	llist_t	*sbuf;
	hasht_t *conn_dispatch_table;
	conn_udp_t default_conn;
} socket_udp_t;

/* The connection struct */
typedef struct conn_udp_st {
	socket_udp_t *sock;
	char peer_host[40];
	uint16_t peer_port;		// In host byte-order
	struct sockaddr peer_addr;
	socklen_t peer_addrlen;
	struct timeval recvtimeo, sendtimeo;
	struct timeval build_tv, first_c_tv;
	struct timeval first_r_tv, first_s_tv;
	struct timeval last_r_tv, last_s_tv;
    int64_t rcount_packet, scount_packet;
    int64_t rcount_byte, scount_byte;
	llist_t	*rbuf, *sbuf;
} conn_udp_t;

socket_udp_t *socket_udp_new(struct sockaddr *bind_addr);
int socket_udp_delete(socket_udp_t *);
//cJSON *socket_udp_serialize(socket_udp_t *);

/*
 * Try to register a connection for a specific peer address to a udp_socket.
 * Nonblocked.
 */
int conn_udp_register_nb(conn_udp_t**, socket_udp_t *sock, struct sockaddr *peer_addr;);

/*
 * Receive/send data from/to a connection struct.
 * Nonblocked.
 */
ssize_t conn_udp_recv_nb(conn_udp_t*, void *buf, size_t size);
ssize_t conn_udp_send_nb(conn_udp_t*, const void *buf, size_t size);


/*
 * Send/receivce data vector to/from a connection.
 */
ssize_t conn_udp_sendv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);
ssize_t conn_udp_recvv_nb(conn_tcp_t *conn, buffer_list_t *bl, size_t size);

/*
 * Flush and close a connection.
 */
int conn_udp_close_nb(conn_udp_t *);

cJSON *conn_udp_serialize(conn_udp_t *);

#endif

