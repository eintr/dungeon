#include <stdio.h>
#include <unistd.h>
#include <netinet/in.h>
#include <errno.h>
#include <pthread.h>

#include "cJSON.h"
#include "util_log.h"
#include "dungeon.h"
#include "ds_state_dict.h"
#include "conf.h"

static pthread_t tid_monitor;
static int monitor_sd;

static void thr_monitor_cleanup_cJSON(void *ptr)
{
	cJSON *p=ptr;
	cJSON_Delete(p);
}

static void thr_monitor_cleanup_fd(void *ptr)
{
	int *p=ptr;
	close(*p);
}

static void *thr_monitor(void *ptr)
{
	cJSON *status;
	int newsd;
	struct sockaddr_in c_addr;
	socklen_t c_addr_len;

	c_addr_len = sizeof(c_addr);
	while (1) {
		newsd = accept(monitor_sd, &c_addr, &c_addr_len);
		if (newsd<0) {
			if (errno==EINTR || errno==EAGAIN) {
				continue;
			}
			mylog(L_WARNING, "monitor connection failed.");
			continue;
		}
		pthread_cleanup_push(thr_monitor_cleanup_fd, &newsd);

		status = cJSON_CreateObject();
		pthread_cleanup_push(thr_monitor_cleanup_cJSON, status);

		cJSON_AddItemToObject(status, "ProxyPool", dungeon_serialize());
		cJSON_AddItemToObject(status, "ServerDict", state_dict_serialize());

		cJSON_fdPrint(newsd, status);

		pthread_cleanup_pop(1);	// cJSON_Delete()
		pthread_cleanup_pop(1);	// close()
	}
	pthread_exit(NULL);
}

int aa_monitor_init(void)
{
	int port, ret, err, val;
	struct sockaddr_in local_addr;

	monitor_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (monitor_sd<0) {
		ret = -errno;
		mylog(L_ERR, "socket(): %m");
		return ret;
	}

	val=1;
	if (setsockopt(monitor_sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val))) {
		mylog(L_WARNING, "Can't set monitor_sd SO_REUSEADDR");
	}

	port = conf_get_monitor_port();
	if (port<0) {
		mylog(L_WARNING, "Can't get port from conf, using default %d", DEFAULT_MONITOR_PORT);
		port = DEFAULT_MONITOR_PORT;
	}

	local_addr.sin_family = AF_INET;
	local_addr.sin_addr.s_addr = 0;
	local_addr.sin_port = htons(port);
	if (bind(monitor_sd, &local_addr, sizeof(local_addr))) {
		ret = -errno;
		mylog(L_ERR, "bind(): %m");
		return ret;
	}

	if (listen(monitor_sd, 1)) {
		ret = -errno;
		mylog(L_ERR, "listen(): %m");
		return ret;
	}

	err = pthread_create(&tid_monitor, NULL, thr_monitor, NULL);
	if (err) {
		mylog(L_ERR, "pthread_create(): %s", strerror(err));
		return -err;
	}

	return 0;
}

int aa_monitor_destroy(void)
{
	pthread_cancel(tid_monitor);
	pthread_join(tid_monitor, NULL);
	close(monitor_sd);
	mylog(L_DEBUG, "Monitor thread terminated.");
	return 0;
}

