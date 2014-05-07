/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cJSON.h>
#include <sys/eventfd.h>
/** \endcond */

#include <room_service.h>

#include <util_log.h>
//#include <util_conn_udp.h>

#include "export.h"

enum state_enum {
	STATE_WORK=1,
	STATE_DIE
};

typedef struct udp_register_st {
	int state;
	struct sockaddr_storage local_addr;	// used for index
    socklen_t local_addrlen;
    int sys_socket;
    llist_t *sbuf;		// Of struct udp_dgram_st*
	udp_wrapper_t wrapper;
	/** \todo llist_t *wrapper_list;	// Multiple datagram target */
} memory_t;

static cJSON	*mod_conf=NULL;
static int module_inited = 0;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;

static void *fsm_new(void *unused);

static imp_soul_t soul;
static llist_t *udp_list=NULL;


static int if_conf_enabled(cJSON *conf)
{
	cJSON *enabled;

	enabled = cJSON_GetObjectItem(conf, "Enabled");
	if (enabled == NULL) {
		mylog(L_WARNING, "[%s]: Configure is not valid, disable module.", MOD_NAME);
		return 0;
	}
	if (enabled->type == cJSON_False) {
		mylog(L_DEBUG, "[%s]: Configured to be disabled.", MOD_NAME);
		return 0;
	} else if (enabled->type == cJSON_True) {
		mylog(L_DEBUG, "[%s]: Configured to be enabled.", MOD_NAME);
		return 1;
	} else {
		mylog(L_WARNING, "[%s]: Configure is not valid, disable module.", MOD_NAME);
		return 0;
	}
	return 0;
}

static void do_mod_init(void)
{
	uint32_t id;
	memory_t *mem;

	udp_list = llist_new(65535);
	if (udp_list == NULL) {
		mylog(L_ERR, "[%s]: malloc(): %m.", MOD_NAME);
	}
}

static int mod_init(cJSON *conf)
{
	mylog(L_INFO, "[%s] is loading.", MOD_NAME);
	mod_conf = conf;

	if (if_conf_enabled(conf)) {
		pthread_once(&init_once, do_mod_init);
		if (udp_list==NULL) {
			return -1;
		}
	}
	return 0;
}

static int mod_destroy(void)
{
	mylog(L_INFO, "[%s] is unloading.", MOD_NAME);
	// Delete all relevant imps
	llist_delete(udp_list);
	return 0;
}

static void mod_maint(void)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
}

static cJSON *mod_serialize(void)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

udp_wrapper_t *register_udp_socket(struct sockaddr *local_addr, socklen_t len)
{
	int sd;
	memory_t *memory;

	// return error if duplicated socket found.

	memory = fsm_new(NULL);
	if (memory!=NULL) {
		if (local_addr!=NULL) {
			if (bind(memory->sys_socket, local_addr, len)<0) {
				mylog(L_ERR, "bind(): %m");
				close(memory->sys_socket);
				free(memory);
				return NULL;
			}
			memcpy(&memory->local_addr, local_addr, len);
			memory->wrapper.local_addrlen = len;
		}
		fsm_new(memory);
		/** \todo register the memory->fds into imp_body's epoll_fd */
		imp_summon(memory, &soul);
	}
	llist_append(udp_list, memory);
	return &memory->wrapper;
}


///////////// FSM /////////////

static void *fsm_new(void *unused)
{
	memory_t *mem;

	mem = malloc(sizeof(*mem));
	if (mem==NULL) {
		return NULL;
	}
	mem->sys_socket = socket(AF_INET, SOCK_DGRAM, 0);
	/** \todo Set up tvs */
	memset(&mem->local_addr, '\0', sizeof(mem->local_addr));
	mem->local_addrlen = 0;
	mem->wrapper.local_addr = (void*)&mem->local_addr;
	mem->wrapper.local_addrlen = 0;
	mem->wrapper.rcount_packet = 0;
	mem->wrapper.scount_packet = 0;
	mem->wrapper.rcount_byte = 0;
	mem->wrapper.scount_byte = 0;
	mem->wrapper.rbuf = llist_new(10);
	mem->wrapper.sbuf = llist_new(10);
	mem->wrapper.r_eventfd = eventfd(0, EFD_NONBLOCK);
	mem->wrapper.s_eventfd = eventfd(0, EFD_NONBLOCK);

	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	mem->state = STATE_WORK;
	return NULL;
}

static int fsm_delete(void *m)
{
	memory_t *mem=m;
	mylog(L_DEBUG, "%s is running, free memory.\n", __FUNCTION__);
	close(mem->sys_socket);
	close(mem->wrapper.r_eventfd);
	close(mem->wrapper.s_eventfd);
	// \todo llist_delete
	free(mem);
	return 0;
}

static enum enum_driver_retcode fsm_driver(void *m)
{
	/** \todo Use event */
	static count =10;
	memory_t *mem=m;
	int i, ret;
	struct udp_dgram_st *dgram=NULL, *rbufp;
	uint8_t rbuf[65535];

	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	for (i=0;;++i) {
		if (llist_fetch_head_nb(mem->wrapper.sbuf, (void*)&dgram)<0) {
			break;
		}
		ret = sendto(mem->sys_socket, dgram->data, dgram->datalen,  MSG_DONTWAIT, (void*)&dgram->peer_addr, dgram->peer_addrlen);
		if (ret>0) {
			mem->wrapper.scount_packet++;
			mem->wrapper.scount_byte += ret;
		}
		free(dgram);
		dgram=NULL;
	}
	/// \todo update last_s_tv, first_s_tv
	fprintf(stderr, "%d msgs sent.\n", i);

	for (i=0;;++i) {
		rbufp = (void*)rbuf;
		rbufp->peer_addrlen = sizeof(struct sockaddr_storage);
		ret = recvfrom(mem->sys_socket, rbufp->data, sizeof(rbuf)-sizeof(struct udp_dgram_st),  MSG_DONTWAIT, (void*)&rbufp->peer_addr, &rbufp->peer_addrlen);
		if (ret<0) {
			break;
		}
		dgram = malloc(sizeof(struct udp_dgram_st)+ret);
		memcpy(dgram, rbufp, sizeof(struct udp_dgram_st)+ret);
		llist_append_nb(mem->wrapper.rbuf, dgram);
		mem->wrapper.rcount_packet++;
		mem->wrapper.rcount_byte += ret;
	}
	/// \todo update last_r_tv, first_r_tv
	fprintf(stderr, "%d msgs recved.\n", i);

	return TO_RUN;
}

static void *fsm_serialize(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

static imp_soul_t soul = {
	.fsm_new = fsm_new,
	.fsm_delete = fsm_delete,
	.fsm_driver = fsm_driver,
	.fsm_serialize = fsm_serialize,
};

module_interface_t MODULE_INTERFACE_SYMB = 
{
	.mod_initializer = mod_init,
	.mod_destroier = mod_destroy,
	.mod_maintainer = mod_maint,
	.mod_serialize = mod_serialize,
};

