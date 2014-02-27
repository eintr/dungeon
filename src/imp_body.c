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

#include "imp_body.h"
#include "util_syscall.h"
#include "ds_state_dict.h" 
#include "util_atomic.h"
#include "conf.h"
#include "util_err.h"
#include "util_log.h"

uint32_t global_context_id___=1;

cJSON *imp_body_serialize(imp_body_t *my)
{
	cJSON *result;

	result = cJSON_CreateObject();

	cJSON_AddNumberToObject(result, "Id", my->id);
	cJSON_AddItemToObject(result, "Info", my->brain->fsm_serialize(my->context_spec_data));

	return result;
}

