#ifndef UTIL_MUTEX_H
#define UTIL_MUTEX_H

typedef int mutex_t;

#define	MUTEX_UNLOCKED	0
#define	MUTEX_LOCKED	1

void mutex_lock(mutex_t*);
void mutex_unlock(mutex_t*);
int mutex_test(mutex_t*);

#endif

