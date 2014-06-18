/** \cond 0 */
#include <stdio.h>
#include <stdint.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "cJSON.h"
/** \endcond */


#include "conf.h"

static cJSON *global_conf;

static int conf_check_legal(cJSON *config);

static cJSON *conf_create_default_config(void)
{
	cJSON *conf;

	conf = cJSON_CreateObject();

	if (DEFAULT_DEBUG_VALUE) {
		cJSON_AddItemToObject(conf, "Debug", cJSON_CreateTrue());
	} else {
		cJSON_AddItemToObject(conf, "Debug", cJSON_CreateFalse());
	}
	cJSON_AddNumberToObject(conf, "MonitorPort", DEFAULT_MONITOR_PORT);
	cJSON_AddNumberToObject(conf, "ConcurrentMax", DEFAULT_CONCURRENT_MAX);
	cJSON_AddStringToObject(conf, "WorkingDir", INSTALL_PREFIX);
	cJSON_AddStringToObject(conf, "ModuleDir", DEFAULT_MODPATH);
	cJSON_AddStringToObject(conf, "Workers", DEFAULT_WORKERS);

	return conf;
}

int conf_new(const char *filename)
{
	FILE *fp;
	cJSON *conf;
	int ret = 0;

	if (filename != NULL) {
		fp = fopen(filename, "r");
		if (fp == NULL) {
			//log error
			fprintf(stderr, "fopen(): %m");
			return -1;
		} 
		conf = cJSON_fParse(fp);
		fclose(fp);
		ret = conf_load_json(conf);
	} else {
		global_conf = conf_create_default_config();
	}

	return ret;
}

int conf_delete()
{
	if (global_conf) {
		cJSON_Delete(global_conf);
		global_conf = NULL;
		return 0;
	}
	return -1;
}


int conf_reload(const char *filename)
{
	FILE *fp;
	cJSON *conf;
	int ret;

	if (filename == NULL) {
		return -1;
	}
	fp = fopen(filename, "r");
	if (fp == NULL) {
		//log error
		return -1;
	}

	conf = cJSON_fParse(fp);
	fclose(fp);

	ret = conf_load_json(conf);
	if (ret < 0) {
		cJSON_Delete(conf);
	}

	return ret;
}

int conf_load_json(cJSON *conf)
{
	if (conf == NULL) {
		//log error;
		return -1;
	}
	if (conf_check_legal(conf) != 0) {
		//log error
		return -1;
	}

	if (global_conf) {
		cJSON_Delete(global_conf);
	}

	global_conf = conf;

	return 0;
}

static int conf_get_concurrent_max_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ConcurrentMax");
	if (tmp) {
		if (tmp->type!=cJSON_Number) {
			return -1;
		}
		return tmp->valueint;
	}
	return DEFAULT_CONCURRENT_MAX;
}

static int conf_get_log_level_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "LogLevel");
	if (tmp) {
		if (tmp->type!=cJSON_Number) {
			return -1;
		}
		return tmp->valueint;
	}
	return 6;
}

static int conf_get_daemon_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "Daemon");
	if (tmp) {
		if (tmp->type == cJSON_True) {
			return 1;
		} else {
			return 0;
		}
	}
	return DEFAULT_DAEMON;
}

static int conf_get_workers_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "Workers");
	if (tmp) {
		if (tmp->type == cJSON_Number) {
			return tmp->valueint;
		} else {
			return 0;
		}
	}
	return DEFAULT_WORKERS;
}

static int conf_get_monitor_port_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "MonitorPort");
	if (tmp) {
		if (tmp->type!=cJSON_Number) {
			return -1;
		}
		return tmp->valueint;
	}
	return DEFAULT_MONITOR_PORT;
}

static char *conf_get_working_dir_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "WorkingDir");
	if (tmp) {
		if (tmp->type!=cJSON_String) {
			return NULL;
		}
		return tmp->valuestring;
	}
	return DEFAULT_WORK_DIR;
}

static char *conf_get_module_dir_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ModuleDir");
	if (tmp) {
		if (tmp->type!=cJSON_String) {
			return NULL;
		}
		return tmp->valuestring;
	}
	return "./";
}

static cJSON *conf_get_modules_desc_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "Modules");
	if (tmp) {
		if (tmp->type!=cJSON_Array) {
			return NULL;
		}
	}
	return tmp;
}


static int conf_check_legal(cJSON *conf)
{
	int LogLevel, ConcurrentMax;
	char *ModuleDir;
	cJSON *Modules;

	LogLevel = conf_get_log_level_internal(conf);
	if (LogLevel < 0)
		return -1;

	ConcurrentMax = conf_get_concurrent_max_internal(conf);
	if (ConcurrentMax < 0) 
		return -1;

	ModuleDir = conf_get_module_dir_internal(conf);
	if (ModuleDir==NULL)
		return -1;

	Modules = conf_get_modules_desc_internal(conf);
	if (Modules==NULL)
		return -1;

	return 0;
}

int conf_get_monitor_port()
{
	return conf_get_monitor_port_internal(global_conf);
}
int conf_get_concurrent_max()
{
	return conf_get_concurrent_max_internal(global_conf);
}
int conf_get_log_level()
{
	return conf_get_log_level_internal(global_conf);
}
int conf_get_daemon()
{
	return conf_get_daemon_internal(global_conf);
}
int conf_get_workers()
{
	return conf_get_workers_internal(global_conf);
}
char *conf_get_working_dir()
{
	return conf_get_working_dir_internal(global_conf);
}
char *conf_get_module_dir()
{
	return conf_get_module_dir_internal(global_conf);
}
cJSON *conf_get_modules_desc()
{
	return conf_get_modules_desc_internal(global_conf);
}

#ifdef AA_CONF_TEST
static void conf_show(cJSON *conf)
{
	int listen_port, connect_to, receive_to, send_to;
	char *level;

	if (conf == NULL) {
		return;
	}

	listen_port = conf_get_listen_port_internal(conf);
	level = conf_get_debug_level_internal(conf);
	connect_to = conf_get_connect_timeout_internal(conf);
	receive_to = conf_get_receive_timeout_internal(conf);
	send_to = conf_get_send_timeout_internal(conf);
	printf("listen_port is %d,\ndebug level is %s,\nconnect timeout is %d,\nreceive timeout is %d,\nsend timeout is %d\n", listen_port, level, connect_to, receive_to, send_to);

	return;

}


int main() 
{
	char *path = "./test.conf";
	char *p2 = NULL;
	int ret;
	ret = conf_new(p2);
	//ret = conf_new(path);
	if (ret == 0) {
		conf_show(global_conf);
		conf_delete();
	} else {
		printf("new conf failed\n");
	}
	return 0;
}

#endif
