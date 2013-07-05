#ifndef SERVER_STATE_DICT_H
#define SERVER_STATE_DICT_H
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>

enum {
	SS_UNKNOWN=0,
	SS_OK,
	SS_CONN_FAILURE,
	SS_DNS_FAILURE,
	SS_SLOW,
};

#define HOSTNAMESIZE	64
typedef struct {
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

int server_state_add_default(server_info_t *s, int state);
int server_state_set_state(char *hostname, int state);
int server_state_set_addr(char *hostname, struct sockaddr_in *sa);
struct server_state_st * server_state_get(char *hostname);
int server_state_inc_pcount(server_info_t*);
int server_state_dec_pcount(server_info_t*);
int server_state_destroy();

int server_state_getinfo(server_info_t*, struct server_state_st*);

#endif

