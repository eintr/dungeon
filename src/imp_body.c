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
	imp_body_t *body = NULL;
	struct epoll_event epev;

	body = calloc(sizeof(*body), 1);
	if (body) {
		body->epoll_fd = epoll_create(1);
		if (body->epoll_fd < 0) {
			mylog(L_ERR, "epoll_create failed, %m");
			goto drop_and_fail1;
		}
		body->timer_fd = timerfd_create(CLOCK_REALTIME, TFD_NONBLOCK);
		if (body->timer_fd < 0) {
			mylog(L_ERR, "timerfd create failed, %m");
			goto drop_and_fail2;
		}
		body->event_fd = eventfd(0, EFD_NONBLOCK);
		if (body->event_fd < 0) {
			goto drop_and_fail3;
		}
		/** Register timer_fd into body->epoll_fd */
		epev.events = EPOLLIN;
		epev.data.u64 = EV_MASK_TIMEOUT;
		epoll_ctl(body->epoll_fd, EPOLL_CTL_ADD, body->timer_fd, &epev);

		/** Register event_fd into body->epoll_fd */
		epev.events = EPOLLIN;
		epev.data.u64 = EV_MASK_EVENT;
		epoll_ctl(body->event_fd, EPOLL_CTL_ADD, body->event_fd, &epev);

		/** Register alert_trap into body->epoll_fd */
		epev.events = EPOLLIN;
		epev.data.u64 = EV_MASK_GLOBAL_ALERT;
		epoll_ctl(body->epoll_fd, EPOLL_CTL_ADD, dungeon_heart->alert_trap, &epev);
	}
	return body;

	close(body->event_fd);
drop_and_fail3:
	close(body->timer_fd);
drop_and_fail2:
	close(body->epoll_fd);
drop_and_fail1:
	free(body);
	return NULL;
}

int imp_body_delete(imp_body_t *body)
{
	close(body->event_fd);
	close(body->timer_fd);
	close(body->epoll_fd);
	free(body);
	return 0;
}

int imp_body_set_ioev(imp_body_t *body, int fd, uint32_t events)
{
	struct epoll_event ev;
	int ret;

	ev.events = events|EPOLLERR|EPOLLHUP|EPOLLRDHUP;
	ev.data.u64 = EV_MASK_IO;
	ret = epoll_ctl(body->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (ret < 0) {
		ret = epoll_ctl(body->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
	}
	return ret;
}

int imp_body_set_timer(imp_body_t *body, int ms)
{
	struct itimerspec its;
	struct epoll_event epev;

	its.it_interval.tv_sec = 0;
	its.it_interval.tv_nsec = 0;
	its.it_value.tv_sec = ms/1000;
	its.it_value.tv_nsec = (ms%1000)*1000000;

	timerfd_settime(body->timer_fd, 0, &its, NULL);

	epev.events = EPOLLIN;
	epev.data.u64 = EV_MASK_TIMEOUT;
	assert(epoll_ctl(body->epoll_fd, EPOLL_CTL_MOD, body->timer_fd, &epev)==0);

	return 0;
}

int imp_body_cleanup_timer(imp_body_t *body)
{
	uint64_t buf;

	if (read(body->timer_fd, &buf, sizeof(buf))==sizeof(buf)) {
		return 1;
	}
	return 0;
}

int imp_body_cleanup_event(imp_body_t *body)
{
	uint64_t buf;

	if (read(body->event_fd, &buf, sizeof(buf))==sizeof(buf)) {
		return 1;
	}
	return 0;
}

uint64_t imp_body_get_event(imp_body_t *body)
{
	uint64_t buf, res = 0;
	int ret, i;
	struct epoll_event ev[1024];

	ret = epoll_wait(body->epoll_fd, ev, 1024, 0);
	if (ret>0) {
		fprintf(stderr, "\tGot %d events\n", ret);
		for (i=0;i<ret;++i) {
			res |= ev[i].data.u64;
		}
	}
	return res;
}

cJSON *imp_body_serialize(imp_body_t *body)
{
	return NULL;
}

