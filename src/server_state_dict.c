#include <stdio.h>

#include "aa_hasht.h"

#include "server_state_dict.h"

static int inited=0;
static hasht_t *ht;

static const hashkey_t hashkey = {
	(int)&(((struct server_state_st*)0)->serverinfo),
	sizeof(server_info_t)
};

static void init(void)
{
	ht = hasht_new(NULL, 10000); //TODO: Define the max value according config file.
	if (ht==NULL) {
		return;
	}
	inited = 1;
	return.
}


static struct server_state_st *server_state_add_default(server_info_t *s)
{
	struct server_state_st *item;

	item = malloc(sizeof(*item));
	if (item==NULL) {
		return -1;
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

	if (!inited) {
		init();
	}

	pos = hasht_find_item(ht, &hashkey, info);
	if (pos==NULL) {
		pos = server_state_add_default(info);
		if (pos==NULL) {
			return -1;
		}
	}
	pos->current_state = state;
	gettimeofday(&last_update_time_tv, NULL);
	return 0;
}

