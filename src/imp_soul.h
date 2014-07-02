/** \file imp_soul.h
	Defines the main action of imp
*/

#ifndef IMP_SOUL_H
#define IMP_SOUL_H

enum enum_driver_retcode {
    TO_RUN=1,
    TO_WAIT_IO,
    TO_TERM,
	I_AM_NEO,
};

struct imp_st;
//typedef struct imp_st imp_t;

typedef struct imp_soul_st {
	void *(*fsm_new)(struct imp_st*);						/**< Imp init. Called by imp_summon(), the pointer passed in must point to a completed imp_t(ptr->memory must has been allocated) , this call back could do some init operations about imp_t.  */
	int (*fsm_delete)(struct imp_st*);						/**< Imp delete. \param Should be imp_t* */
	enum enum_driver_retcode (*fsm_driver)(struct imp_st*);	/**< Core action of imp. \param Should be imp_t* */
	void *(*fsm_serialize)(struct imp_st*);					/**< Report imp soul status. \param Should be imp_t* */
} imp_soul_t;

#endif

