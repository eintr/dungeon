#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/time.h>
#include <sys/timerfd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

#include "gen_context.h"
#include "util_syscall.h"
#include "aa_state_dict.h" 
#include "aa_http_status.h"
#include "aa_atomic.h"
#include "aa_conf.h"
#include "util_err.h"
#include "util_log.h"

uint32_t global_context_id___=1;

cJSON *generic_context_serialize(generic_context_t *my)
{
	cJSON *result;

	result = cJSON_CreateObject();

	cJSON_AddNumberToObject(result, "Id", my->id);
	cJSON_AddItemToObject(result, "Info", my->module_iface->fsm_serialize(my->context_spec_data));

	return result;
}

