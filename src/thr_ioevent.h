#ifndef THR_IOEVENT_H
#define THR_IOEVENT_H

/** \cond 0 */
#include <sys/epoll.h>
/** \endcond */

#include "imp.h"

/** Initialize the ioevent engine
	\return 0 if OK.
*/
int thr_ioevent_init(void);

/** Destroy the ioevent engine
*/
void thr_ioevent_destroy(void);

void thr_ioevent_interrupt(void);

#endif

