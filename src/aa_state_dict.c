#include <stdio.h>

#include "aa_hasht.h"

#include "aa_state_dict.h"

static int inited=0;
static hasht_t *ht;

static const hashkey_t hashkey = {
	(int)&(((struct server_state_st*)0)->serverinfo.hostname),
	sizeof(char *)
};

static void server_state_modify(void *item, void *args)
{
	struct server_state_st *state_item;
	int state;
   
	state_item = (struct server_state_st *)item;
	state = *(int *)args;

	state_item->current_state = state;
}

static void server_state_init(void)
{
	ht = hasht_new(NULL, 10000); //TODO: Define the max value according config file.
	if (ht==NULL) {
		return;
	}
	inited = 1;
	return;
}


static struct server_state_st *server_state_add_default(server_info_t *s)
{
	struct server_state_st *item;

	item = malloc(sizeof(*item));
	if (item == NULL) {
		return NULL;
	}

	memset(item, 0, sizeof(*item));

	item->concurrent_max = 100000;
	/* TODO: config
	item->config_c_timeout_tv = 
	item->config_r_timeout_tv = 
	item->config_s_timeout_tv = 
	*/

	memcpy(&item->serverinfo, s, sizeof(server_info_t));
	if (hasht_add_item(ht, &hashkey, item)) {
		return NULL;
	}

	return item;
}

int server_state_set(server_info_t *info, int state)
{
	struct server_state_st *pos;
	struct server_state_st server_state;
	int ret;

	if (!inited) {
		server_state_init();
	}

	memcpy(&server_state.serverinfo, info, sizeof(*info));

	ret = hasht_modify_item(ht, &hashkey, &server_state, server_state_modify, &state);
	if (ret == -1) {
		pos = server_state_add_default(info);
		if (pos == NULL) {
			return -1;
		}
	}
	//gettimeofday(&pos->last_update_time_tv, NULL);
	return 0;
}

struct sockaddr_in* server_state_find(char *hostname)
{
	struct server_state_st tmp_state;
	struct server_state_st *server_state;
	server_info_t info;

	if (!inited) {
		return NULL;
	}

	strncpy(info.hostname, hostname, strlen(hostname));
	memcpy(&tmp_state.serverinfo, &info, sizeof(info));
	
	server_state = hasht_find_item(ht, &hashkey, &tmp_state);
	if (server_state == NULL) {
		return NULL;
	}

	return &server_state->serverinfo.saddr;
}

#ifdef AA_DICT_TEST
#include "aa_test.h"

void functional_test()
{
	int ret;
	char *hn = "not.localhost";
	server_info_t info;

	/*
	 * Test 1
	 */
	server_state_init();
	strncpy(info.hostname, hn, strlen(hn));
	info.saddr.sin_port = 9999;

	ret = server_state_set(&info, SS_OK);
	if (ret == -1) {
		printf("set info failed\n");
	}
	struct sockaddr_in *sa;

	sa = server_state_find(hn);
	if (sa) {
		printf("found %s, port is %d\n", hn, sa->sin_port);
	}
	sa = server_state_find("localhost");
	if (!sa) {
		printf("not found localhost\n");
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
