#ifndef THR_IOEVENT_H
#define THR_IOEVENT_H

#include <sys/epoll.h>

#include "imp.h"

int thr_ioevent_init(void);
void thr_ioevent_destroy(void);

#endif

