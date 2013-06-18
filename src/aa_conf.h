#ifndef AA_CONF_H
#define AA_CONF_H

#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "cJSON.h"

#define APPNAME						"aa_proxy"
#define APPVERSION					"0.1"

#define DEFAULT_LISTEN_PORT			19999
#define DEFAULT_DEBUG_VALUE 		1
#define DEFAULT_CONNECT_TIMEOUT		200
#define DEFAULT_RECEIVE_TIMEOUT		200
#define DEFAULT_SEND_TIMEOUT		200
#define DEFAULT_CONFPATH			"../conf/aa_proxy.conf"

#define PORT_MIN 10000
#define PORT_MAX 65535
#define TIMEOUT_MIN 1
#define TIMEOUT_MAX 300000

//extern cJSON *global_conf;

int conf_new(const char *filename);
int conf_delete();
int conf_reload(const char *filename);
int conf_load_json(cJSON *conf);

char *conf_get_listen_addr();
int conf_get_listen_port();
int conf_get_concurrent_max();
int conf_get_debug();
int conf_get_connect_timeout();
int conf_get_receive_timeout();
int conf_get_send_timeout();
char *conf_get_working_dir(void);

#endif

