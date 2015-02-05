#ifndef UTIL_MISC_H
#define UTIL_MISC_H

/** \cond 0 */
#include <alloca.h>
#include <sys/time.h>
#include <string.h>
/** \endcond */

#include <dungeon.h>

#define	offsetof(T, M)	((intptr_t)(((T*)0)->M))

static inline int delta_t(void)
{
    return time(NULL)-dungeon_heart->create_time.tv_sec;
}

static inline uint32_t systimestamp_ms(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);

	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
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

