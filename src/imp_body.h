/** \file imp_body.h Physical functions of imp */

#ifndef IMP_BODY_H
#define IMP_BODY_H
/** \cond 0 */
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "cJSON.h"
/** \endcond */

typedef struct imp_body_st {
	int timer_fd;
	int epoll_fd;
	int event_fd;

	char *errlog_str;
} imp_body_t;

imp_body_t *imp_body_new(void);
int imp_body_delete(imp_body_t*);

int imp_body_set_ioev(imp_body_t*, int fd, struct epoll_event*);
int imp_body_get_ioev(imp_body_t*, struct epoll_event*);

int imp_body_set_timer(imp_body_t*, int ms);

int imp_body_test_timeout(imp_body_t*);
int imp_body_test_event(imp_body_t*);

cJSON *imp_body_serialize(imp_body_t*);

#endif

