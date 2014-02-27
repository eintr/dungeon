#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>
#include <dlfcn.h>

#include "conf.h"
#include "dungeon.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "module_interface.h"
#include "thr_maintainer.h"
#include "thr_worker.h"

extern int nr_cpus;

static int dungeon_add_rooms(dungeon_t *d);
static int dungeon_add_room(dungeon_t *, const char *fname, cJSON *conf);
static int dungeon_demolish_allrooms(dungeon_t *);
static int dungeon_demolish_room(dungeon_t *, const char *fname);    // TODO



// TODO: Demolish rooms.

dungeon_t *dungeon_new(int nr_workers, int nr_imp_max)
{
	dungeon_t *d;
	int i, err, ret;

	if (nr_workers<1 || nr_imp_max<1) {
		mylog(L_ERR, "Invalid paremeters");
		return NULL;
	}

	d = calloc(1, sizeof(dungeon_t));
	if (d == NULL) {
		mylog(L_ERR, "not enough memory");
		return NULL;
	}

	d->nr_max = nr_imp_max;
	d->nr_total = 0;
	d->nr_workers = nr_workers;
	d->nr_busy_workers = 0;
	d->run_queue = llist_new(nr_imp_max);
	d->terminated_queue = llist_new(nr_imp_max);
	d->epoll_fd = epoll_create(1);
	d->worker = malloc(sizeof(pthread_t)*nr_workers);
	if (d->worker==NULL) {
		mylog(L_ERR, "Not enough memory for worker thread ids.");
		exit(1);
	}
	for (i=0;i<nr_workers;++i) {
		err = pthread_create(d->worker+i, NULL, thr_worker, d);
		if (err) {
			mylog(L_ERR, "Create worker thread failed");
			break;
		}
	}
	if (i==0) {
		mylog(L_ERR, "No worker thread created.");
		abort();
	} else {
		mylog(L_INFO, "%d worker thread(s) created.", i);
	}

	err = pthread_create(&d->maintainer, NULL, thr_maintainer, d);
	if (err) {
		mylog(L_ERR, "Create maintainer thread failed");
		abort();
	}

	d->rooms = llist_new(nr_imp_max);

	if ((ret = dungeon_add_rooms(d))<1) {
		mylog(L_ERR, "No module loaded successfully.");
		abort();
	}
	mylog(L_INFO, "Loaded %d modules from %s.", ret, conf_get_module_dir());

	gettimeofday(&d->create_time, NULL);

	return d;
}

int dungeon_delete(dungeon_t *pool)
{
	register int i;
	imp_body_t *gen_context;
	llist_node_t *curr;

	// Stop all workers.
	if (pool->worker_quit == 0) {
		pool->worker_quit = 1;
		for (i=0;i<pool->nr_workers;++i) {
			pthread_join(pool->worker[i], NULL);
		}
	}

	free(pool->worker);
	pool->worker = NULL;

	// Stop the maintainer.
	if (pool->maintainer_quit == 0) {
		pool->maintainer_quit = 1;
		pthread_join(pool->maintainer, NULL);
	}

	// Unregister all modules.
	dungeon_demolish_allrooms(pool);
	llist_delete(pool->rooms);

	// Destroy terminated_queue.	
	for (curr=pool->terminated_queue->dumb->next;
			curr != pool->terminated_queue->dumb;
			curr=curr->next) {
		gen_context = curr->ptr;
		gen_context->brain->fsm_delete(gen_context);
		curr->ptr = NULL;
	}
	llist_delete(pool->terminated_queue);

	// Destroy run_queue.
	for (curr=pool->run_queue->dumb->next;
			curr != pool->run_queue->dumb;
			curr=curr->next) {
		gen_context = curr->ptr;
		gen_context->brain->fsm_delete(gen_context);
		curr->ptr = NULL;
	}
	llist_delete(pool->run_queue);

	// Close epoll fd;
	close(pool->epoll_fd);
	pool->epoll_fd = -1;

	// Unload all modules
	dungeon_demolish_allrooms(pool);

	// Destroy itself.
	free(pool);

	return 0;
}

static int dungeon_add_room(dungeon_t *pool, const char *fname, cJSON *conf)
{
	module_handler_t *handle;

	handle = module_load_only(fname);
	if (handle==NULL) {
		return -1;
	}
	if (handle->interface->mod_initializer(pool, conf)) {
		exit(1); // FIXME
	}
	return llist_append(pool->rooms, handle);
}

static int dungeon_demolish_room(dungeon_t *pool, const char *fname)
{
	// TODO
	mylog(LOG_WARNING, "Unloading module is not supported yet.");
	return 0;
}

static void do_unload(void *mod)
{
	module_handler_t *p=mod;

	p->interface->mod_destroier(NULL);
	module_unload_only(mod);
}

static int dungeon_demolish_allrooms(dungeon_t *pool)
{
	(void)dungeon_demolish_room;
	return llist_travel(pool->rooms, do_unload);
}

static int dungeon_add_rooms(dungeon_t *d)
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
		if (dungeon_add_room(d, pat, mod_conf)==0) {
			++count;
		} else {
			mylog(L_WARNING, "Module[%d] (%s) load failed.", i, pat);
		}
	}

	return count;
}

void imp_set_run(dungeon_t *pool, imp_body_t *cont)
{
	llist_append(pool->run_queue, cont);
}

void imp_set_iowait(dungeon_t *pool, int fd, imp_body_t *cont)
{
	struct epoll_event ev;

	ev.events = EPOLLIN|EPOLLOUT|EPOLLONESHOT;
	ev.data.ptr = cont;
	epoll_ctl(pool->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void imp_set_term(dungeon_t *pool, imp_body_t *cont)
{
	llist_append(pool->terminated_queue, cont);
}

cJSON *dungeon_serialize(dungeon_t *pool)
{
	cJSON *result;
	struct tm *tm;
	char timebuf[1024], secbuf[16];

	tm = localtime(&pool->create_time.tv_sec);
	strftime(timebuf, 1024, "%Y-%m-%d %H:%M:%S", tm);
	snprintf(secbuf, 16, ".%u", (unsigned int) pool->create_time.tv_usec*1000/1000000);
	strcat(timebuf, secbuf);

	result = cJSON_CreateObject();

	cJSON_AddStringToObject(result, "CreateTime", timebuf);
	cJSON_AddNumberToObject(result, "MaxProxies", pool->nr_max);
	cJSON_AddNumberToObject(result, "TotalProxies", pool->nr_total);
	cJSON_AddNumberToObject(result, "NumWorkerThreads",pool->nr_workers);
	cJSON_AddNumberToObject(result, "NumBusyWorkers", pool->nr_busy_workers);
	cJSON_AddItemToObject(result, "RunQueue", llist_info_json(pool->run_queue));
	cJSON_AddItemToObject(result, "TerminatedQueue", llist_info_json(pool->terminated_queue));

	return result;
}

