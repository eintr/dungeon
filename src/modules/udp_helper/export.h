#ifndef UDP_HELPER_EXPORT_H
#define UDP_HELPER_EXPORT_H

/** \cond 0 */
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdint.h>
#include "cJSON.h"
/** \endcond */

//#include <room_service.h>
#include "ds_llist.h"

struct udp_dgram_st {
	struct sockaddr_storage peer_addr;
	socklen_t peer_addrlen;
	socklen_t datalen;
	uint8_t data[0];
};

/* The UDP wrapper struct */
typedef struct udp_userside_st {
    struct sockaddr *local_addr; // used for debug
    socklen_t local_addrlen;
	struct timeval recvtimeo, sendtimeo;
	struct timeval build_tv, first_c_tv;
	struct timeval first_r_tv, first_s_tv;
	struct timeval last_r_tv, last_s_tv;
    int64_t rcount_packet, scount_packet;
    int64_t rcount_byte, scount_byte;
	llist_t	*rbuf, *sbuf;
	int r_eventfd, s_eventfd;
} udp_userside_t;

udp_userside_t *register_udp_socket(struct sockaddr *local_addr, socklen_t len);
int unregister_udp_socket(udp_userside_t *);
cJSON *udp_socket_serialize(udp_userside_t *);

size_t udp_sendto_nb(udp_userside_t *, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

#endif

