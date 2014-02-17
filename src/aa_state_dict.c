#include <stdio.h>

#include "ds_hasht.h"

#include "aa_state_dict.h"

static int inited=0;
static hasht_t *ht;

static const hashkey_t hashkey = {
	__builtin_offsetof (struct server_state_st, serverinfo.hostname),
	sizeof(((struct server_state_st*)0)->serverinfo.hostname)
};

static void server_state_modify_state(void *item, void *args)
{
	struct server_state_st *state_item;
	int state;
   
	state_item = (struct server_state_st *)item;
	state = *(int *)args;

	state_item->current_state = state;
}

static void server_state_modify_addr(void *item, void *args)
{
	struct server_state_st *state_item;
	struct sockaddr_in *sa;
   
	state_item = (struct server_state_st *)item;
	sa = (struct sockaddr_in *)args;

	memcpy(&state_item->serverinfo.saddr, sa, sizeof(struct sockaddr_in));
}

static int server_state_init(void)
{
	ht = hasht_new(NULL, 10000); //TODO: Define the max value according config file.
	if (ht == NULL) {
		return -1;
	}
	inited = 1;
	return 0;
}

int server_state_destroy()
{
	if (ht) {
		hasht_clean_table(ht);
		hasht_delete(ht);
		ht = NULL;
	}
	return 0;
}

int server_state_add_default(server_info_t *s, int state)
{
	struct server_state_st *item;
	int ret;

	if (!inited) {
		ret = server_state_init();
		if (ret == -1) {
			return -1;
		}
	}

	item = malloc(sizeof(*item));
	if (item == NULL) {
		return -1;
	}

	memset(item, 0, sizeof(*item));

	item->concurrent_max = 100000;
	item->current_state = state;
	/* TODO: config
	item->config_c_timeout_tv = 
	item->config_r_timeout_tv = 
	item->config_s_timeout_tv = 
	*/

	memcpy(&item->serverinfo, s, sizeof(server_info_t));
	if (hasht_add_item(ht, &hashkey, item)) {
		free(item);
		return -1;
	}

	return 0;
}

int server_state_set_state(char *hostname, int state)
{
	struct server_state_st server_state;
	server_info_t info;
	int ret;

	if (!inited) {
		ret = server_state_init();
		if (ret == -1) {
			return -1;
		}
	}

	memset(info.hostname, 0, sizeof(info.hostname));
	strncpy(info.hostname, hostname, strlen(hostname));
	memcpy(&server_state.serverinfo, &info, sizeof(info));

	ret = hasht_modify_item(ht, &hashkey, &server_state, server_state_modify_state, &state);
	if (ret == -1) {
		return -1;
	}
	//gettimeofday(&pos->last_update_time_tv, NULL);
	return 0;
}

int server_state_set_addr(char *hostname, struct sockaddr_in *sa)
{
	struct server_state_st server_state;
	server_info_t info;
	int ret;

	if (!inited) {
		ret = server_state_init();
		if (ret == -1) {
			return -1;
		}
	}

	memset(info.hostname, 0, sizeof(info.hostname));
	strncpy(info.hostname, hostname, strlen(hostname));
	memcpy(&server_state.serverinfo, &info, sizeof(info));

	ret = hasht_modify_item(ht, &hashkey, &server_state, server_state_modify_addr, sa);
	if (ret == -1) {
		return -1;
	}
	//gettimeofday(&pos->last_update_time_tv, NULL);
	return 0;
}

struct server_state_st* server_state_get(char *hostname)
{
	struct server_state_st tmp_state;
	server_info_t info;

	if (!inited) {
		return NULL;
	}

	memset(info.hostname, 0, sizeof(info.hostname));
	strncpy(info.hostname, hostname, strlen(hostname));
	memcpy(&tmp_state.serverinfo, &info, sizeof(info));

	return hasht_find_item(ht, &hashkey, &tmp_state);
}

static cJSON *translater(void *ptr)
{
	struct server_state_st *p = ptr;
	cJSON *result;
	char ip4string[16];

	inet_ntop(AF_INET, &p->serverinfo.saddr.sin_addr.s_addr, ip4string, 16);

	result = cJSON_CreateObject();
	cJSON_AddStringToObject(result, "Hostname", p->serverinfo.hostname);
	cJSON_AddStringToObject(result, "Address", ip4string);
	/* TODO: Latency output
	cJSON *latencies;
	latencies = cJSON_CreateArray();
	cJSON_AddItemToObject(result, "Latency", latencies);
	*/

	return result;
}

cJSON *state_dict_serialize(void)
{
	cJSON *result;

	if (ht!=NULL) {
		result = hasht_info_cjson(ht, translater);
	} else {
		result = cJSON_CreateNull();
	}

	return result;
}

#ifdef AA_DICT_TEST
#include "aa_test.h"

void functional_test()
{
	int ret;
	char *hn1 = "10.73.31.119:8080", *hn2 = "10.73.31.119:5050";
	server_info_t info;
	struct server_state_st *ss;

	/*
	 * Test 1
	 */
	server_state_init();
	memset(info.hostname, 0, sizeof(info.hostname));
	strncpy(info.hostname, hn1, strlen(hn1));
	info.saddr.sin_port = 9999;

	ret = server_state_add_default(&info, SS_OK);
	if (ret == -1) {
		printf("add default info failed\n");
	}

	ss = server_state_get(hn1);
	if (ss) {
		printf("found %s, port is %d, state is %d\n", hn1, ss->serverinfo.saddr.sin_port, ss->current_state);
	}

	ret = server_state_set_state(hn1, SS_DNS_FAILURE);
	if (ret == -1) {
		printf("set info failed\n");
	}
	ss = server_state_get(hn1);
	if (ss) {
		printf("found %s, port is %d, state is %d\n", hn1, ss->serverinfo.saddr.sin_port, ss->current_state);
	}

	ss = server_state_get(hn2);
	if (!ss) {
		printf("not found %s\n", hn2);
	} else {
		printf("found %s, port is %d, state is %d\n", ss->serverinfo.hostname, ss->serverinfo.saddr.sin_port, ss->current_state);
	}

	printf("%stest1 successful%s\n", COR_BEGIN, COR_END);

	return;
}

int main()
{
	functional_test();
	return 0;
}

#endif
