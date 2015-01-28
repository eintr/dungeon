/** \file imp_body.h Physical functions of imp -- deprecated! */
#if 0
#ifndef IMP_BODY_H
#define IMP_BODY_H
/** \cond 0 */
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include "cJSON.h"
/** \endcond */

#define EV_MASK_TIMEOUT			0x80000000UL
#define EV_MASK_KILL			0x40000000UL
#define EV_MASK_GLOBAL_KILL		0x20000000UL

int imp_body_set_ioev(imp_body_t*, int fd, uint32_t events);
int imp_body_set_timeout(imp_body_t*, int ms);

int imp_body_cancel_timeout(imp_body_t *body);

#endif
#endif

