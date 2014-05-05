/**
	\file thr_maintainer.h
	Maintainer thread function
*/

#ifndef THR_MAINTAINER_H
#define THR_MAINTAINER_H

/** Create maintainer thread */
int thr_maintainer_create(void);

/** Delete maintainer thread */
int thr_maintainer_destroy(void);

/** Wake up the maintainer thread instantly */
void thr_maintainer_wakeup(void);

#endif

