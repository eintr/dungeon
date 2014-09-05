/** \file util_timer.h
	Defined some syscalls those were not suppored by current glibc
*/
#ifndef TIMER_H
#define TIMER_H

/** \cond 0 */
#include <sys/types.h>
/** \endcond */

enum timer_status_en {
	TIMER_OK=0,
	TIMER_OVER=1,
	TIMER_CANCELED,
	TIMER_NOEXIST,
};

typedef void timer_callback_func_t(void*);

/** Initiate the timer facility.
	\return 0 if OK.
*/
int timer_facility_init(void);
int timer_facility_destroy(void);

void* timer_set_rel(int ms, timer_callback_func_t*, void*);
//void* timer_set_abs(int ms, timer_callback_func_t*, void*);
int timer_cancel(void*);
int timer_drop(void*);

#endif

