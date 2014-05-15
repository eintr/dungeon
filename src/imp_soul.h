/** \file imp_soul.h
	Defines the main action of imp
*/

#ifndef IMP_SOUL_H
#define IMP_SOUL_H

enum enum_driver_retcode {
    TO_RUN=1,
    TO_WAIT_IO,
    TO_TERM,
};

struct imp_st;
typedef struct imp_st imp_t;

typedef struct imp_soul_st {
	void *(*fsm_new)(imp_t*);						/**< Imp init. Called by imp_summon(), the pointer passed in must point to a completed imp_t(ptr->memory must has been allocated) , this call back could do some init operations about imp_t.  */
	int (*fsm_delete)(imp_t*);						/**< Imp delete */
	enum enum_driver_retcode (*fsm_driver)(imp_t *);	/**< Core action of imp */
	void *(*fsm_serialize)(imp_t*);					/**< Report imp soul status */
} imp_soul_t;

#endif

