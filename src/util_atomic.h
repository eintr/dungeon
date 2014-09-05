/**
	\file util_atomic.h
	Defined some useful atomic operations for multithread parallel
*/

#ifndef UTIL_ATOMIC_H
#define UTIL_ATOMIC_H

/*
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
*/

/*
static inline int atomic_increase(int *value)
{
	return do_fetch_and_add(value, 1); 
}

static inline int atomic_decrease(int *value)
{
	return do_fetch_and_add(value, -1);
}
*/

#define atomic_increase(P)	__sync_fetch_and_add (P, 1)
#define atomic_decrease(P)	__sync_fetch_and_add (P, -1)
#define atomic_fetch_and_add(P, N)	__sync_fetch_and_add (P, N)
#define atomic_fetch_and_add64	atomic_fetch_and_add

#define atomic_cas(ptr, oldv, newv) __sync_bool_compare_and_swap(ptr, oldv, newv)

#endif

