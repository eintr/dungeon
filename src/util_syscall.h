/** \file util_syscall.h
	Defined some syscalls those were not suppored by current glibc
*/
#ifndef SYSCALL_H
#define SYSCALL_H

/** \cond 0 */
#include <sys/types.h>
/** \endcond */

/** Get LWP PID
	\return The LWP PID of current process.
*/
pid_t gettid(void);

#endif

