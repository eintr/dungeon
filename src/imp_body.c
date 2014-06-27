/** \file imp_body.c Physical functions of imp */

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
		/** Register timer_fd and event_fd into imp epoll_fd */
		epev.events = EPOLLIN;
		epev.data.u64 = EV_MASK_TIMEOUT;
		epoll_ctl(imp->epoll_fd, EPOLL_CTL_ADD, imp->timer_fd, &epev);
		epev.data.u64 = EV_MASK_EVENT;
		epoll_ctl(imp->event_fd, EPOLL_CTL_ADD, imp->event_fd, &epev);

		/** Register alert_trap into imp epoll_fd */
		epev.events = EPOLLIN;
		epev.data.u64 = EV_MASK_GLOBAL_ALERT;
		epoll_ctl(imp->epoll_fd, EPOLL_CTL_ADD, dungeon_heart->alert_trap, &epev);

		/** Register imp epoll_fd into dungeon_heart's epoll_fd */
		epev.events = 0;
		epev.data.ptr = imp;
		epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, imp->epoll_fd, &epev);
	}
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
	close(imp_body->event_fd);
	close(imp_body->timer_fd);
	close(imp_body->epoll_fd);
	free(imp_body);
	return 0;
}

int imp_body_set_ioev(imp_body_t *body, int fd, uint32_t events)
{
	struct epoll_event ev;
	int ret;

	ev.events = events;
	ev.data.u64 = EV_MASK_IO;
	ret = epoll_ctl(body->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (ret < 0) {
		ret = epoll_ctl(body->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	}
	return ret;
}

uint64_t imp_body_get_event(imp_body_t *body)
{
	uint64_t res = 0;
	int ret, i;
	struct epoll_event ev[10];

	while (1) {
		ret = epoll_wait(body->epoll_fd, ev, 10, 0);
		if (ret<=0) {
			break;
		}
		for (i=0;i<ret;++i) {
			res |= ev[i].data.u64;
		}
	}
	return ret;
}

int imp_body_set_timer(imp_body_t *body, int ms)
{
	struct itimerspec its;

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = ms/1000;
	its.it_value.tv_nsec = (ms%1000)*1000000;

	return timerfd_settime(body->timer_fd, 0, &its, NULL);
}

int imp_body_test_timeout(imp_body_t *body)
{
	uint64_t buf;

	if (read(body->timer_fd, &buf, sizeof(buf))==sizeof(buf)) {
		return 1;
	}
	return 0;
}

int imp_body_test_event(imp_body_t *body)
{
	uint64_t buf;

	if (read(body->event_fd, &buf, sizeof(buf))==sizeof(buf)) {
		return 1;
	}
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

