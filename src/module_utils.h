#ifndef AA_MODULE_UTILS_H
#define AA_MODULE_UTILS_H

void proxt_pool_set_run(imp_pool_t *, generic_context_t *);

// Used by module
generic_context_t *generic_context_assemble(void *context_spec_data, module_interface_t  *module_iface);

#endif

