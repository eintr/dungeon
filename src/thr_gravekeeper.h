/**
	\file thr_gravekeeper.h
	Bury those dead imps.
*/

#ifndef THR_GRAVEKEEPER_H
#define THR_GRAVEKEEPER_H

/** Create gravekeeper thread */
int thr_gravekeeper_create(void);

/** Delete gravekeeper thread */
int thr_gravekeeper_destroy(void);

/** Wake up the gravekeeper thread instantly */
void thr_gravekeeper_wakeup(void);

#endif

