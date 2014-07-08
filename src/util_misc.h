#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <dungeon.h>

#define	offsetof(T, M)	((intptr_t)(((T*)0)->M))

static inline int delta_t(void)
{
    return time(NULL)-dungeon_heart->create_time.tv_sec;
}

#endif

