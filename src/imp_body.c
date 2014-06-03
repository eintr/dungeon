/** \cond 0 */
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
/** \endcond */

#include "imp_body.h"
#include "util_syscall.h"
#include "ds_state_dict.h" 
#include "util_atomic.h"
#include "conf.h"
#include "util_err.h"
#include "util_log.h"
#include "dungeon.h"

#define	EV64_SHIFT	0x8000000000000000ULL
#define	EV64_TIMER	(EV64_SHIFT+1)
#define	EV64_EVENT	(EV64_SHIFT+2)

imp_body_t *imp_body_new(void)
{
	imp_body_t *imp = NULL;
	struct epoll_event epev;

	imp = calloc(sizeof(*imp), 1);
	if (imp) {
		imp->epoll_fd = epoll_create(1);
		if (imp->epoll_fd < 0) {
			mylog(L_ERR, "epoll_create failed, %m");
			goto drop_and_fail1;
		}
		imp->timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
		if (imp->timer_fd < 0) {
			mylog(L_ERR, "timerfd create failed, %m");
			goto drop_and_fail2;
		}
		imp->event_fd = eventfd(0, EFD_NONBLOCK);
		if (imp->event_fd < 0) {
			goto drop_and_fail3;
		}
		memset(&imp->epoll_ev, 0, sizeof(imp->epoll_ev));
	}
	/** Register timer_fd and event_fd into imp epoll_fd */
	epev.events = EPOLLIN;
	epev.data.u64 = EV64_TIMER;
	epoll_ctl(imp->epoll_fd, EPOLL_CTL_ADD, imp->timer_fd, &epev);
	epev.data.u64 = EV64_EVENT;
	epoll_ctl(imp->event_fd, EPOLL_CTL_ADD, imp->event_fd, &epev);

	/** Register imp epoll_fd into dungeon_heart's epoll_fd */
	epev.events = EPOLLIN|EPOLLONESHOT;
	epev.data.ptr = imp;
	epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, imp->epoll_fd, &epev);
	return imp;

	close(imp->event_fd);
drop_and_fail3:
	close(imp->timer_fd);
drop_and_fail2:
	close(imp->epoll_fd);
drop_and_fail1:
	free(imp);
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
	return epoll_ctl(body->epoll_fd, EPOLL_CTL_MOD, fd, ev);
}

int imp_body_get_ioev(imp_body_t *body, struct epoll_event *ev)
{
	return 0;
}

int imp_body_set_timer(imp_body_t *body, const struct itimerval *itv)
{
	struct itimerspec its;

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = itv->it_interval.tv_sec;
	its.it_value.tv_nsec = itv->it_interval.tv_usec*1000;

	return timerfd_settime(body->timer_fd, 0, &its, NULL);
}

cJSON *imp_body_serialize(imp_body_t *my)
{
	cJSON *result;

	result = cJSON_CreateObject();

//	cJSON_AddNumberToObject(result, "Id", my->id);
//	cJSON_AddItemToObject(result, "Info", my->soul->fsm_serialize(my->context_spec_data));

	return result;
}

