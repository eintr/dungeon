#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "imp_pool.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "module_interface.h"
#include "thr_maintainer.h"
#include "thr_worker.h"

extern int nr_cpus;

imp_pool_t *imp_pool_new(int nr_workers, int nr_max)
{
	imp_pool_t *pool;
	int i, err;

	if (nr_workers<1 || nr_max<1) {
		mylog(L_ERR, "Invalid paremeters");
		return NULL;
	}

	pool = calloc(1, sizeof(imp_pool_t));
	if (pool == NULL) {
		mylog(L_ERR, "not enough memory");
		return NULL;
	}

	pool->nr_max = nr_max;
	pool->nr_total = 0;
	pool->nr_workers = nr_workers;
	pool->nr_busy_workers = 0;
	pool->run_queue = llist_new(nr_max);
	pool->terminated_queue = llist_new(nr_max);
	pool->epoll_fd = epoll_create(1);
	pool->worker = malloc(sizeof(pthread_t)*nr_workers);
	if (pool->worker==NULL) {
		mylog(L_ERR, "Not enough memory for worker thread ids.");
		exit(1);
	}
	for (i=0;i<nr_workers;++i) {
		err = pthread_create(pool->worker+i, NULL, thr_worker, pool);
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

	err = pthread_create(&pool->maintainer, NULL, thr_maintainer, pool);
	if (err) {
		mylog(L_ERR, "Create maintainer thread failed");
		abort();
	}

	pool->modules = llist_new(nr_max);

	gettimeofday(&pool->create_time, NULL);

	return pool;
}

int imp_pool_delete(imp_pool_t *pool)
{
	register int i;
	generic_context_t *gen_context;
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
	imp_pool_unload_allmodule(pool);
	llist_delete(pool->modules);

	// Destroy terminated_queue.	
	for (curr=pool->terminated_queue->dumb->next;
			curr != pool->terminated_queue->dumb;
			curr=curr->next) {
		gen_context = curr->ptr;
		gen_context->module_iface->fsm_delete(gen_context);
		curr->ptr = NULL;
	}
	llist_delete(pool->terminated_queue);

	// Destroy run_queue.
	for (curr=pool->run_queue->dumb->next;
			curr != pool->run_queue->dumb;
			curr=curr->next) {
		gen_context = curr->ptr;
		gen_context->module_iface->fsm_delete(gen_context);
		curr->ptr = NULL;
	}
	llist_delete(pool->run_queue);

	// Close epoll fd;
	close(pool->epoll_fd);
	pool->epoll_fd = -1;

	// Unload all modules
	imp_pool_unload_allmodule(pool);

	// Destroy itself.
	free(pool);

	return 0;
}

int imp_pool_load_module(imp_pool_t *pool, const char *fname, cJSON *conf)
{
	module_handler_t *handle;

	handle = module_load_only(fname);
	if (handle==NULL) {
		return -1;
	}
	if (handle->interface->mod_initializer(pool, conf)) {
		exit(1); // FIXME
	}
	return llist_append(pool->modules, handle);
}

int imp_pool_unload_module(imp_pool_t *pool, const char *fname)
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

int imp_pool_unload_allmodule(imp_pool_t *pool)
{
	return llist_travel(pool->modules, do_unload);
}


void imp_set_run(imp_pool_t *pool, generic_context_t *cont)
{
	llist_append(pool->run_queue, cont);
}

void imp_set_iowait(imp_pool_t *pool, int fd, generic_context_t *cont)
{
	struct epoll_event ev;

	ev.events = EPOLLIN|EPOLLOUT|EPOLLONESHOT;
	ev.data.ptr = cont;
	epoll_ctl(pool->epoll_fd, EPOLL_CTL_MOD, fd, &ev);
}

void imp_set_term(imp_pool_t *pool, generic_context_t *cont)
{
	llist_append(pool->terminated_queue, cont);
}

cJSON *imp_pool_serialize(imp_pool_t *pool)
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

