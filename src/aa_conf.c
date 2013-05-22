#include "aa_conf.h"


static int conf_check_legal(cJSON *config);

static cJSON *conf_create_default_config(void)
{
	cJSON *conf;

	conf = cJSON_CreateObject();

	cJSON_AddNumberToObject(conf, "ListenPort", DEFAULT_LISTEN_PORT);
	cJSON_AddStringToObject(conf, "Debug", DEFAULT_DEBUG_LEVEL);
	cJSON_AddNumberToObject(conf, "ConnectTimeout_ms", DEFAULT_CONNECT_TIMEOUT);
	cJSON_AddNumberToObject(conf, "ReceiveTimeout_ms", DEFAULT_RECEIVE_TIMEOUT);
	cJSON_AddNumberToObject(conf, "SendTimeout_ms", DEFAULT_SEND_TIMEOUT);

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
	// TODO:
	return 20000;
}

static char * conf_get_debug_level_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "Debug");
	if (tmp) {
		return tmp->valuestring;
	}
	return NULL;
}

static int conf_get_connect_timeout_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ConnectTimeout_ms");
	if (tmp) {
		return tmp->valueint;
	}
	return -1;
}
static int conf_get_receive_timeout_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ReceiveTimeout_ms");
	if (tmp) {
		return tmp->valueint;
	}
	return -1;
}
static int conf_get_send_timeout_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "SendTimeout_ms");
	if (tmp) {
		return tmp->valueint;
	}
	return -1;
}

static int conf_get_listen_port_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ListenPort");
	if (tmp) {
		return tmp->valueint;
	}
	return -1;
}

static char * conf_get_listen_addr_internal(cJSON *conf)
{
	cJSON *tmp;
	tmp = cJSON_GetObjectItem(conf, "ListenAddr");
	if (tmp) {
		return tmp->valuestring;
	}
	return NULL;
}

static int conf_check_legal(cJSON *conf)
{
	int listen_port, connect_to, receive_to, send_to;

	listen_port = conf_get_listen_port_internal(conf);
	if (listen_port < PORT_MIN || listen_port > PORT_MAX)
		return -1;

	connect_to = conf_get_connect_timeout_internal(conf);
	if (connect_to < TIMEOUT_MIN || connect_to > TIMEOUT_MAX) 
		return -1;

	receive_to = conf_get_receive_timeout_internal(conf);
	if (receive_to < TIMEOUT_MIN || receive_to > TIMEOUT_MAX)
		return -1;

	send_to = conf_get_send_timeout_internal(conf);
	if (send_to < TIMEOUT_MIN || send_to > TIMEOUT_MAX)
		return -1;

	return 0;
}

char * conf_get_listen_addr()
{
	return conf_get_listen_addr_internal(global_conf);
}
int conf_get_listen_port()
{
	return conf_get_listen_port_internal(global_conf);
}
int conf_get_concurrent_max()
{
	return conf_get_concurrent_max_internal(global_conf);
}
char * conf_get_debug_level()
{
	return conf_get_debug_level_internal(global_conf);
}
int conf_get_connect_timeout()
{
	return conf_get_connect_timeout_internal(global_conf);
}
int conf_get_receive_timeout()
{
	return conf_get_receive_timeout_internal(global_conf);
}
int conf_get_send_timeout()
{
	return conf_get_send_timeout_internal(global_conf);
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
