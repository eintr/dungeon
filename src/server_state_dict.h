#ifndef SERVER_STATE_DICT_H
#define SERVER_STATE_DICT_H

enum {
	SS_OK=0,
	SS_FAILURE,
	SS_SLOW,
	SS_UNKNOWN
};

#define HOSTNAMESIZE	64
typedef union {
	struct sockaddr_in saddr;
	char hostname[HOSTNAMESIZE];
} server_info_t;

struct serer_state_st {
	server_info_t serverinfo;
	struct timeval config_c_timeout_tv, config_r_timeout_tv,config_s_timeout_tv;
	int current_state, proxy_count;
	struct timeval last_update_time_tv;
	struct timeval last_update_interval_tv;
};

int server_state_set(struct serer_state_st *);
int server_state_inc_pcount(struct serer_state_st *);
int server_state_dec_pcount(struct serer_state_st *);

#endif

