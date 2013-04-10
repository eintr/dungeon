#ifndef SERVER_STATE_DICT_H
#define SERVER_STATE_DICT_H

enum {
	SS_OK=0,
	SS_FAILURE,
	SS_UNKNOWN
};

struct serer_state_result_st {
	int state;
	struct timeval since_tv;
};

int server_state_register();

#endif

