/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
//#include <time.h>
#include <sys/time.h>
#include <dlfcn.h>
/** \endcond */

#include "conf.h"
#include "dungeon.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "thr_maintainer.h"
#include "thr_worker.h"
#include "imp.h"

extern int nr_cpus;

static int dungeon_add_rooms(void);
static int dungeon_add_room(const char *fname, cJSON *conf);
static int dungeon_demolish_allrooms(void);
static int dungeon_demolish_room(const char *fname);    // TODO

dungeon_t *dungeon_heart=NULL;

// TODO: Demolish rooms.

int dungeon_init(int nr_workers, int nr_imp_max)
{
	int i, err, ret;

	if (nr_workers<1 || nr_imp_max<1) {
		mylog(L_ERR, "Invalid paremeters");
		return EINVAL;
	}

	dungeon_heart = calloc(1, sizeof(dungeon_t));
	if (dungeon_heart == NULL) {
		mylog(L_ERR, "not enough memory");
		return ENOMEM;
	}

	dungeon_heart->nr_max = nr_imp_max;
	dungeon_heart->nr_total = 0;
	dungeon_heart->nr_workers = nr_workers;
	dungeon_heart->nr_busy_workers = 0;
	dungeon_heart->run_queue = llist_new(nr_imp_max);
	dungeon_heart->terminated_queue = llist_new(nr_imp_max);
	dungeon_heart->epoll_fd = epoll_create(1);
	dungeon_heart->worker = malloc(sizeof(pthread_t)*nr_workers);
	if (dungeon_heart->worker==NULL) {
		mylog(L_ERR, "Not enough memory for worker thread ids.");
		return ENOMEM;
	}
	for (i=0;i<nr_workers;++i) {
		err = pthread_create(dungeon_heart->worker+i, NULL, thr_worker, (void*)i);
		if (err) {
			mylog(L_ERR, "Create worker thread failed");
			break;
		}
	}
	if (i==0) {
		mylog(L_ERR, "No worker thread created.");
		return EAGAIN;
	} else {
		mylog(L_INFO, "%d worker thread(s) created.", i);
	}

	err = thr_maintainer_create();
	if (err) {
		mylog(L_ERR, "Create maintainer thread failed");
		return EAGAIN;
	}

	gettimeofday(&dungeon_heart->create_time, NULL);

	dungeon_heart->rooms = llist_new(nr_imp_max);

	if ((ret = dungeon_add_rooms())<1) {
		mylog(L_ERR, "No module loaded successfully.");
		return EAGAIN;
	}
	mylog(L_INFO, "Loaded %d modules from %s.", ret, conf_get_module_dir());

	return 0;
}

int dungeon_delete(void)
{
	register int i;
	imp_t *imp;
	llist_node_t *curr;

	// Stop all workers.
	if (dungeon_heart->worker_quit == 0) {
		dungeon_heart->worker_quit = 1;
		for (i=0;i<dungeon_heart->nr_workers;++i) {
			pthread_join(dungeon_heart->worker[i], NULL);
		}
	}
	mylog(L_DEBUG, "dungeon worker thread stopped.");

	free(dungeon_heart->worker);
	dungeon_heart->worker = NULL;

	// Stop the maintainer.
	thr_maintainer_destroy();
	mylog(L_DEBUG, "dungeon maintainer thread stopped.");

	// Destroy terminated_queue.	
	for (curr=dungeon_heart->terminated_queue->dumb.next;
			curr != &dungeon_heart->terminated_queue->dumb;
			curr=curr->next) {
		imp = curr->ptr;
		imp->soul->fsm_delete(imp);
		imp_dismiss(imp);
		curr->ptr = NULL;
	}
	llist_delete(dungeon_heart->terminated_queue);
	mylog(L_DEBUG, "dungeon terminated_queue destroyed.");

	// Destroy run_queue.
	for (curr=dungeon_heart->run_queue->dumb.next;
			curr != &dungeon_heart->run_queue->dumb;
			curr=curr->next) {
		imp = curr->ptr;
		imp->soul->fsm_delete(imp);
		imp_dismiss(imp);
		curr->ptr = NULL;
	}
	llist_delete(dungeon_heart->run_queue);
	mylog(L_DEBUG, "dungeon run_queue destroyed.");

	// Close epoll fd;
	close(dungeon_heart->epoll_fd);
	dungeon_heart->epoll_fd = -1;
	mylog(L_DEBUG, "dungeon epoll_fd closed.");

	// Unload all modules
	dungeon_demolish_allrooms();
	llist_delete(dungeon_heart->rooms);
	mylog(L_DEBUG, "all dungeon rooms destroyed.");

	// Destroy itself.
	free(dungeon_heart);
	dungeon_heart = NULL;

	return 0;
}

static int dungeon_add_room(const char *fname, cJSON *conf)
{
	module_handler_t *handle;

	handle = module_load_only(fname);
	if (handle==NULL) {
		return -1;
	}
	if (handle->interface->mod_initializer(conf)) {
		exit(1); // FIXME
	}
	return llist_append(dungeon_heart->rooms, handle);
}

static int dungeon_demolish_room(const char *fname)
{
	// TODO
	mylog(LOG_WARNING, "Unloading module is not supported yet.");
	return 0;
}

static void do_unload(void *mod)
{
	module_handler_t *p=mod;

	p->interface->mod_destroier();
	module_unload_only(mod);
}

static int dungeon_demolish_allrooms()
{
	(void)dungeon_demolish_room;
	return llist_travel(dungeon_heart->rooms, do_unload);
}

static int dungeon_add_rooms(void)
{
	int i;
	char pat[1024];
	int count=0;
	cJSON *conf_mods, *mod_desc, *mod_conf, *mod_path;

	conf_mods = conf_get_modules_desc();
	if (conf_mods==NULL) {
		mylog(L_ERR, "No module description.");
		abort();
	}
	if (conf_mods->type != cJSON_Array) {
		mylog(L_ERR, "Module description section has to be an ARRAY.");
		abort();
	}
	for (i=0;i<cJSON_GetArraySize(conf_mods);++i) {
		mod_desc = cJSON_GetArrayItem(conf_mods, i);
		mod_path = cJSON_GetObjectItem(mod_desc, "Path");
		if (mod_path->type != cJSON_String) {
			mylog(L_WARNING, "Module[%d] path is not a string.", i);
			continue;
		}
		mod_conf = cJSON_GetObjectItem(mod_desc, "Config");
		if (mod_conf->type != cJSON_Object) {
			mylog(L_WARNING, "Module[%d] conf type is not a JSON.", i);
			continue;
		}
		if (mod_path->valuestring[0]!='/') {
			snprintf(pat, 1024, "%s/%s", conf_get_module_dir(), mod_path->valuestring);
		} else {
			snprintf(pat, 1024, "%s", mod_path->valuestring);
		}
		mylog(L_INFO, "Loading Module[%d] %s.", i, pat);
		if (dungeon_add_room(pat, mod_conf)==0) {
			++count;
		} else {
			mylog(L_WARNING, "Module[%d] (%s) load failed.", i, pat);
		}
	}

	return count;
}

cJSON *dungeon_serialize(void)
{
	cJSON *result;
	struct tm *tm;
	char timebuf[1024], secbuf[16];

	tm = localtime(&dungeon_heart->create_time.tv_sec);
	strftime(timebuf, 1024, "%Y-%m-%d %H:%M:%S", tm);
	snprintf(secbuf, 16, ".%u", (unsigned int) dungeon_heart->create_time.tv_usec*1000/1000000);
	strcat(timebuf, secbuf);

	result = cJSON_CreateObject();

	cJSON_AddStringToObject(result, "CreateTime", timebuf);
	cJSON_AddNumberToObject(result, "MaxProxies", dungeon_heart->nr_max);
	cJSON_AddNumberToObject(result, "TotalProxies", dungeon_heart->nr_total);
	cJSON_AddNumberToObject(result, "NumWorkerThreads", dungeon_heart->nr_workers);
	cJSON_AddNumberToObject(result, "NumBusyWorkers", dungeon_heart->nr_busy_workers);
	cJSON_AddItemToObject(result, "RunQueue", llist_info_json(dungeon_heart->run_queue));
	cJSON_AddItemToObject(result, "TerminatedQueue", llist_info_json(dungeon_heart->terminated_queue));

	return result;
}

