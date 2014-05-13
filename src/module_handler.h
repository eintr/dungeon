/**	\file module_handler.h
	A basic wrap of room module.
*/

#ifndef fsm_module_hub_h
#define fsm_module_hub_h

#include "room_template.h"

#define	PATHSIZE	256

/** A room module wrap struct */
typedef struct {
	char mod_path[PATHSIZE];	/**< Module path */
	void *mod_handler;			/**< Module handler returned by dlopen() */
	module_interface_t *interface;	/**< Module interface */
} module_handler_t;

/** Load a room module without interface actions. */
module_handler_t *module_load_only(const char *fname);

/** Unload a loaded room module */
void module_unload_only(module_handler_t *);

#endif

