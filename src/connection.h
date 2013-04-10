#ifndef CONNECTION_H
#define CONNECTION_H

typedef struct connection_st {
	char *peer_host;
	uint16_t peer_port;		// In host byte-order
	struct sockaddr *peer_addr, *local_addr;
	socklen_t peer_addrlen, local_addrlen;
	struct timeval connecttimeo, recvtimeo, sendtimeo;
	struct timeval build_tv, first_c_tv;
	struct timeval first_r_tv, first_s_tv;
	struct timeval last_r_tv, last_s_tv;
	int sd;
} connection_t;

int aa_connection_accept_nb(connection_t**, int listen_sd);

int aa_connection_connect_nb(connection_t**, char *peer_host, uint16_t peer_port);

ssize_t aa_connection_read_nb(connection_t*, void *buf, size_t size);
ssize_t aa_connection_write_nb(connection_t*, const void *buf, size_t size);

int aa_connection_close_nb(connection_t *);

char *aa_connection_cstr(connection_t *);

#endif

