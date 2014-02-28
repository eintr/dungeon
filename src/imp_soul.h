#ifndef IMP_SOUL_H
#define IMP_SOUL_H

enum enum_driver_retcode {
    TO_RUN=1,
    TO_WAIT_IO,
    TO_TERM,
};

typedef struct imp_soul_st {
	void *(*fsm_new)(void*);
	int (*fsm_delete)(void*);
	enum enum_driver_retcode (*fsm_driver)(void*);
	void *(*fsm_serialize)(void*);
} imp_soul_t;

imp_soul_t *imp_soul_new(void);
int imp_soul_delete(imp_soul_t *);

#endif

