#ifndef GENERIC_CONTEXT_H
#define GENERIC_CONTEXT_H

#include <stdint.h>

#include "cJSON.h"

#include "proxy_pool.h"

typedef struct generic_context_st {
	uint32_t id;
	proxy_pool_t *pool;

	char *errlog_str;

	void *context_spec_data;
	module_interface_t	*module_iface;
} generic_context_t;

extern uint32_t global_context_id___;

#define GET_CONTEXT_ID __sync_fetch_and_add(&global_context_id___, 1)

cJSON *generic_context_serialize(generic_context_t*);

#endif

