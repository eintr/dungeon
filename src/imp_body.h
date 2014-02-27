#ifndef IMP_BODY_H
#define IMP_BODY_H

#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "module_interface.h"

#include "cJSON.h"

struct dungeon_st;

typedef struct imp_body_st {
	uint32_t id;

	int timer_fd;
	int epoll_fd;
	struct epoll_event epoll_ev;

	char *errlog_str;

	void *context_spec_data;
	imp_soul_t	*brain;
} imp_body_t;

extern uint32_t global_context_id___;
#define GET_NEXT_CONTEXT_ID __sync_fetch_and_add(&global_context_id___, 1)

// Used by module
//imp_body_t *imp_body_assemble(void *context_spec_data, module_interface_t  *module_iface);

int imp_body_reg_ioev(imp_body_t*, int fd, struct epoll_event*);
int imp_body_get_ioev(imp_body_t*, struct epoll_event*);

int imp_body_set_timer(imp_body_t*, struct itimerval);

cJSON *imp_body_serialize(imp_body_t*);

#endif

