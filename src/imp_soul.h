/** \file imp_soul.h
	Defines the main action of imp
*/

#ifndef IMP_SOUL_H
#define IMP_SOUL_H

enum enum_driver_retcode {
    TO_RUN=129,
    TO_BLOCK,
    TO_TERM,
	I_AM_NEO,
};

struct imp_st;
//typedef struct imp_st imp_t;

typedef struct imp_soul_st {
	void *(*fsm_new)(void*);			/**< Imp init. Called by imp_summon(), \param Should be imp's memory*, this call back could do some init operations about imp's memory*.  */
	int (*fsm_delete)(void*);			/**< Imp delete. \param Should be imp's memory* */
	enum enum_driver_retcode (*fsm_driver)(void*);	/**< Core action of imp. \param Should be imp's memory* */
	void *(*fsm_serialize)(void*);		/**< Report imp soul status. \param Should be imp's memory* */
} imp_soul_t;

#endif

