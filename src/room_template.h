#ifndef fsm_module_interface_h
#define fsm_module_interface_h

#include <stdio.h>
#include "cJSON.h"

#define MODULE_INTERFACE_SYMB	mod_interface
#define MODULE_INTERFACE_SYMB_STR	"mod_interface"

typedef struct {
	int (*mod_initializer)(void /*dungeon_t*/ *d, cJSON *config);	// Arg is considered to be configuration.
	int (*mod_destroier)(void*);
	void (*mod_maintainer)(void*);
	cJSON *(*mod_serialize)(void*);
} module_interface_t;

#endif

