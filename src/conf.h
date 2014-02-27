#ifndef AA_CONF_H
#define AA_CONF_H

#include "cJSON.h"

//#define APPNAME						"aa_proxy"
#define APPVERSION					APPVERSION_MAJ "." APPVERSION_MIN

#define DEFAULT_DAEMON				1
#define	DEFAULT_CONCURRENT_MAX		20000
#define DEFAULT_MONITOR_PORT		19990
#define DEFAULT_DEBUG_VALUE 		1
#define	DEFAULT_WORK_DIR			INSTALL_PREFIX
#define DEFAULT_CONFPATH			CONFDIR"/"APPNAME".conf"
#define DEFAULT_MODPATH				MODDIR

#define PORT_MIN 1025
#define PORT_MAX 65535

#define BACKLOG_NUM 500

// TODO: Thread priorities.

int conf_new(const char *filename);
int conf_delete();
int conf_reload(const char *filename);
int conf_load_json(cJSON *conf);

int conf_get_monitor_port();
int conf_get_concurrent_max();
int conf_get_log_level();
int conf_get_daemon();
char *conf_get_working_dir(void);
char *conf_get_module_dir(void);
cJSON *conf_get_modules_desc(void);

#endif

