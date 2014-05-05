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

typedef struct imp_soul_st {
	void *(*fsm_new)(void*);						/**< Imp new */
	int (*fsm_delete)(void*);						/**< Imp delete */
	enum enum_driver_retcode (*fsm_driver)(void*);	/**< Core action of imp */
	void *(*fsm_serialize)(void*);					/**< Report imp soul status */
} imp_soul_t;

//imp_soul_t *imp_soul_new(void);
//int imp_soul_delete(imp_soul_t *);

#endif

