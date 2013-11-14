#ifndef fsm_module_interface_h
#define fsm_module_interface_h

#include "cJSON.h"

enum enum_driver_retcode {
	TO_RUN=1,
	TO_WAIT_IO,
	TO_TERM,
}

typedef struct {
	int (*mod_initializer)(cJSON_t *);	// Arg is considered to be configuration.
	int (*mod_destroier)(void*);
	void (*mod_maintainer)(void*);
	cJSON_t *mod_serialize(void*);

	void *fsm_new(void*);
	int fsm_delete(void*);
	enum enum_driver_retcode fsm_driver(void*);
	cJSON_t *fsm_serialize(void*);
} module_interface_t;

#endif

