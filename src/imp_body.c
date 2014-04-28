#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/epoll.h>

#include "imp_body.h"
#include "util_syscall.h"
#include "ds_state_dict.h" 
#include "util_atomic.h"
#include "conf.h"
#include "util_err.h"
#include "util_log.h"

imp_body_t *imp_body_new(void)
{
	imp_body_t *imp = NULL;

	imp = malloc(sizeof(*imp));
	if (imp) {
		memset(imp, 0, sizeof(*imp));
		imp->epoll_fd = epoll_create(1);
		if (imp->epoll_fd < 0) {
			mylog(L_ERR, "epoll_create failed, %m");
			goto drop_and_fail;
		}
		imp->timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
		if (imp->timer_fd < 0) {
			mylog(L_ERR, "timerfd create failed, %m");
			goto drop_and_fail1;
		}
		imp->epoll_fd = epoll_create(255);
		if (imp->epoll_fd < 0) {
			goto drop_and_fail2;
		}
		imp->event_fd = eventfd(0, EFD_NONBLOCK);
		if (imp->event_fd < 0) {
			goto drop_and_fail3;
		}
		memset(&imp->epoll_ev, 0, sizeof(imp->epoll_ev));
	}
	return imp;

	close(imp->event_fd);
drop_and_fail3:
	close(imp->epoll_fd);
drop_and_fail2:
	close(imp->timer_fd);
drop_and_fail1:
	free(imp);
drop_and_fail:
	return NULL;
}

int imp_body_delete(imp_body_t *imp_body)
{
	close(imp_body->timer_fd);
	close(imp_body->epoll_fd);
	free(imp_body);
	return 0;
}

int imp_body_set_ioev(imp_body_t *body, int fd, struct epoll_event *ev)
{
	return 0;
}

int imp_body_get_ioev(imp_body_t *body, struct epoll_event *ev)
{
	return 0;
}

int imp_body_set_timer(imp_body_t *body, struct itimerval itv)
{
	return 0;
}

cJSON *imp_body_serialize(imp_body_t *my)
{
	cJSON *result;

	result = cJSON_CreateObject();

//	cJSON_AddNumberToObject(result, "Id", my->id);
//	cJSON_AddItemToObject(result, "Info", my->soul->fsm_serialize(my->context_spec_data));

	return result;
}

