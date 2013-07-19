#ifndef AA_CONF_H
#define AA_CONF_H

#include "cJSON.h"

#define APPNAME						"aa_proxy"
#define APPVERSION					"0.1"

#define DEFAULT_LISTEN_PORT			19999
#define DEFAULT_MONITOR_PORT		19990
#define DEFAULT_DEBUG_VALUE 		1
#define DEFAULT_CONNECT_TIMEOUT		200
#define DEFAULT_RECEIVE_TIMEOUT		200
#define DEFAULT_SEND_TIMEOUT		200
#define DEFAULT_CONFPATH			CONFDIR"/"APPNAME".conf"

#define PORT_MIN 10000
#define PORT_MAX 65535
#define TIMEOUT_MIN 1
#define TIMEOUT_MAX 300000

int conf_new(const char *filename);
int conf_delete();
int conf_reload(const char *filename);
int conf_load_json(cJSON *conf);

char *conf_get_listen_addr();
int conf_get_listen_port();
int conf_get_monitor_port();
int conf_get_concurrent_max();
int conf_get_log_level();
int conf_get_daemon();
int conf_get_connect_timeout();
int conf_get_receive_timeout();
int conf_get_send_timeout();
char *conf_get_working_dir(void);

#endif

