/**
	\file util_atomic.h
	Defined some useful atomic operations for multithread parallel
*/

#ifndef UTIL_ATOMIC_H
#define UTIL_ATOMIC_H

static inline int do_fetch_and_add(int *value, int add) 
{
	__asm__ volatile (

		"lock;"
		"    xaddl  %0, %1;   "

		: "+r" (add) : "m" (*value) : "cc", "memory");

	return add;
}

static inline uint64_t atomic_fetch_and_add64(uint64_t *ptr, int64_t d)
{
	return __sync_fetch_and_add (ptr, d);
}

static inline int atomic_increase(int *value)
{
	return do_fetch_and_add(value, 1); 
}

static inline int atomic_decrease(int *value)
{
	return do_fetch_and_add(value, -1);
}

#endif

