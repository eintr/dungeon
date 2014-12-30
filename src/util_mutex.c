#include "util_mutex.h"
#include "util_atomic.h"

void mutex_lock(mutex_t *m)
{
	while (!atomic_cas(m, MUTEX_UNLOCKED, MUTEX_LOCKED));
}

void mutex_unlock(mutex_t *m)
{
	*m = MUTEX_UNLOCKED;
}

int mutex_test(mutex_t *m)
{
	return *m;
}

