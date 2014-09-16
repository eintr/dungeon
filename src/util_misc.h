#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <stdio.h>
#include <string.h>
#include <alloca.h>

#include "dungeon.h"

#define	offsetof(T, M)	((intptr_t)(((T*)0)->M))

#define	TRUE	1
#define	FALSE	0

static inline int delta_t(void)
{
    return time(NULL)-dungeon_heart->create_time.tv_sec;
}

static inline cJSON *cpuset_to_cjson(cpu_set_t *cpuset, int max)
{
    int i;
    char *result;

    result = alloca(max+1);
    memset(result, '.', max);
    result[max] = 0;
    for (i=0;i<max;++i) {
        if (CPU_ISSET(i, cpuset)) {
            result[i]='O';
        }
    }
    return cJSON_CreateString(result);
}

void timespec_normalize(struct timespec*);

struct timespec timespec_now(struct timespec*);

struct timespec timespec_sub(struct timespec*, struct timespec *);

struct timespec timespec_add(struct timespec*, struct timespec *);

struct timespec timespec_add_ms(struct timespec*, int ms);

struct timespec timespec_by_ms(int ms);

#endif

