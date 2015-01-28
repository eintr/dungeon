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

int imp_body_set_ioev(imp_body_t *body, int fd, uint32_t events)
{
	struct epoll_event ev;
	int ret;

	ev.events = events|EPOLLERR|EPOLLHUP|EPOLLRDHUP;
	ev.data.u64 = EV_MASK_IO;
	ret = epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	if (ret < 0) {
		ret = epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_ADD, fd, &ev);
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
	assert(epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, body->timer_fd, &epev)==0);

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

	ret = epoll_wait(dungeon_heart->epoll_fd, ev, 1024, 0);
	if (ret>0) {
		for (i=0;i<ret;++i) {
			res |= ev[i].data.u64;
		}
	}
	return res;
}

#if 0
ssize_t read_op(int fd, void *buf, size_t bufsize, int nextstate)
{
	ssize_t ret;
	struct epoll_event epev;

	while(1) {
		ret = read(fd, buf, bufsize);
		if (ret>=0) {
			return ret;
		}
		if (ret<0) {
			if (errno==EINTR) {
				continue;
			}
			if (errno==EAGAIN) {
				epev.events = EPOLLIN | EPOLLONESHOT;
				epev.data.ptr = 
				epoll_ctl(dungeon_heart->epoll_fd, EPOLL_CTL_MOD, );
			}
		}
	}
}
#endif

cJSON *imp_body_serialize(imp_body_t *body)
{
	return NULL;
}

