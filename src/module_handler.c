#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <string.h>

#include "module_handler.h"
#include "util_log.h"

module_handler_t *module_load_only(const char *fname)
{
	module_handler_t *res;
	void *handler;

	handler = dlopen(fname, RTLD_NOW|RTLD_GLOBAL|RTLD_DEEPBIND);
	if (handler==NULL) {
		mylog(L_ERR, "dlopen(): %s", dlerror());
		return NULL;
	}

	res = malloc(sizeof(*res));
	if (res==NULL) {
		mylog(L_ERR, "malloc(): %m");
		return NULL;
	}
	res->mod_path[PATHSIZE-1]='\0';
	strncpy(res->mod_path, fname, PATHSIZE);

	res->mod_handler = handler;

	res->interface = dlsym(handler, MODULE_INTERFACE_SYMB_STR);
	if (res->interface==NULL) {
		mylog(L_ERR, "Can't resolv " MODULE_INTERFACE_SYMB_STR " in %s", fname);
		dlclose(handler);
		free(res);
		return NULL;
	}

	return res;
}

void module_unload_only(module_handler_t *mod)
{
	mod->interface = NULL;
	dlclose(mod->mod_handler);
	free(mod);
}

