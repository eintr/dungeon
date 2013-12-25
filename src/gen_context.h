#ifndef GENERIC_CONTEXT_H
#define GENERIC_CONTEXT_H

#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "aa_module_interface.h"

#include "cJSON.h"

struct proxy_pool_st;

typedef struct generic_context_st {
	uint32_t id;
//	struct proxy_pool_st *pool;

	int timer_fd;
	int epoll_fd;
	struct epoll_event epoll_ev;

	char *errlog_str;

	void *context_spec_data;
	module_interface_t	*module_iface;
} generic_context_t;

extern uint32_t global_context_id___;
#define GET_CONTEXT_ID __sync_fetch_and_add(&global_context_id___, 1)

// Used by module
//generic_context_t *generic_context_assemble(void *context_spec_data, module_interface_t  *module_iface);

int generic_context_reg_ioev(generic_context_t*, int fd, struct epoll_event*);
int generic_context_get_ioev(generic_context_t*, struct epoll_event*);

int generic_context_set_timer(generic_context_t*, struct itimerval);

cJSON *generic_context_serialize(generic_context_t*);

#endif

