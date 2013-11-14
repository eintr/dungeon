#ifndef fsm_module_hub_h
#define fsm_module_hub_h

#include "aa_module_interface.h"

#define	MODULE_INTERFACE_SYMB	"mod_interface"

typedef struct {
	char mod_path[PATHSIZE];
	void *mod_handler;
	module_interface_t	*interface;
} module_handler_t;

module_handler_t *module_load_only(const char *fname);
void module_unload_only(module_handler_t *);

#endif

