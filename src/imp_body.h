/** \file imp_body.h Physical functions of imp */

#ifndef IMP_BODY_H
#define IMP_BODY_H
/** \cond 0 */
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "cJSON.h"
/** \endcond */

#define EV_MASK_KILL			0x0000000000000001ULL
#define EV_MASK_TIMEOUT			0x0000000000000002ULL
#define EV_MASK_EVENT			0x0000000000000004ULL
#define EV_MASK_IO				0x0000000000000008ULL
#define EV_MASK_IOERR			0x0000000000000010ULL
#define EV_MASK_GLOBAL_MASK		0x8000000000000000ULL
#define EV_MASK_GLOBAL_ALERT	(EV_MASK_GLOBAL_MASK|0x0000000000000001ULL)

typedef struct imp_body_st {
	int timer_fd;
	int epoll_fd;
	int event_fd;

	char *errlog_str;
} imp_body_t;

imp_body_t *imp_body_new(void);
int imp_body_delete(imp_body_t*);

int imp_body_set_ioev(imp_body_t*, int fd, uint32_t events);
int imp_body_set_timer(imp_body_t*, int ms);

uint64_t imp_body_get_event(imp_body_t*);

cJSON *imp_body_serialize(imp_body_t*);

#endif

