#ifndef SERVER_STATE_DICT_H
#define SERVER_STATE_DICT_H

enum {
	SS_UNKNOWN=0,
	SS_OK,
	SS_CONN_FAILURE,
	SS_DNS_FAILURE,
	SS_SLOW,
};

#define HOSTNAMESIZE	64
typedef union {
	struct sockaddr_in saddr;
	char hostname[HOSTNAMESIZE];
} server_info_t;

struct server_state_st {
	server_info_t serverinfo;
	struct timeval config_c_timeout_tv, config_r_timeout_tv,config_s_timeout_tv;
	struct timeval latency_tv;
	int current_state, concurrent_count, concurrent_max;
	struct timeval last_update_time_tv;
};

int server_state_set(server_info_t*, int);
int server_state_inc_pcount(server_info_t*);
int server_state_dec_pcount(server_info_t*);

int server_state_getinfo(server_info_t*, struct server_state_st*);

#endif

