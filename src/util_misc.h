#ifndef UTIL_MISC_H
#define UTIL_MISC_H

#include <dungeon.h>
#include <alloca.h>

#define	offsetof(T, M)	((intptr_t)(((T*)0)->M))

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

#endif

