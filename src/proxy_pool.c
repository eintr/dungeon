#include <stdio.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "proxy_pool.h"
#include "aa_syscall.h"
#include "aa_err.h"
#include "aa_log.h"
#include "aa_module_handler.h"
#include "aa_module_interface.h"

extern int nr_cpus;

static void run_context(aa_context_t *node)
{
	int ret;
	ret = node->module_iface->fsm_driver(node);
	if (ret==TO_RUN) {
		proxt_pool_set_run(node->pool, node);
	} else if (ret==TO_WAIT_IO) {
		proxt_pool_set_iowait(node->pool, node->epoll_fd);
	} else if (ret==TO_TERM) {
		proxt_pool_set_term(node->pool, node);
	} else {
		mylog(L_ERR, "FSM driver returned bad code %d, this must be a BUG!\n", ret);
		proxt_pool_set_term(node->pool, node);	// Terminate the BAD fsm.
	}
}

void *thr_worker(void *p)
{
	const int IOEV_SIZE=nr_cpus;
	proxy_pool_t *pool=p;
	aa_context_t *node;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];
	sigset_t allsig;
	int i;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!pool->worker_quit) {
		/* deal node in run queue */
		while (1) {
			err = llist_fetch_head_nb(pool->run_queue, (void **)&node);
			if (err < 0) {
				//mylog(L_DEBUG, "run_queue is empty.");
				break;
			}
			mylog(L_DEBUG, "fetch from run queue node is %p\n", node);
			run_context(node);
		}

		num = epoll_wait(pool->epoll_pool, ioev, IOEV_SIZE, 1000);
		if (num<0) {
			//mylog(L_ERR, "epoll_wait(): %s", strerror(errno));
		} else if (num==0) {
			//mylog(L_DEBUG, "epoll_wait() timed out.");
		} else {
			for (i=0;i<num;++i) {
				mylog(L_DEBUG, "epoll: context[%u] has event %u", ((proxy_context_t*)(ioev[i].data.ptr))->id, ioev[i].events);
				run_context(ioev[i].data.ptr);
			}
		}
	}

	return NULL;
}

void *thr_maintainer(void *p)
{
	proxy_pool_t *pool=p;
	proxy_context_t *node;
	sigset_t allsig;
	int i, err;
	struct timespec tv;

	tv.tv_sec = 2;
	tv.tv_nsec = 0;

	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);

	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!pool->maintainer_quit) {
		
		/* deal nodes in terminated queue */
		for (i=0;;++i) {
			err = llist_fetch_head_nb(pool->terminated_queue, (void **)&node);
			if (err < 0) {
				break;
			}
			mylog(L_DEBUG, "fetch context[%u] from terminate queue", node->id);
			run_context(node);
		}
		if (i==0) {
			//mylog(L_DEBUG, "%u : terminated_queue is empty.\n", gettid());
			// May be dynamically increase the sleep interval.
		} else {
			// May be dynamically decrease the sleep interval.
		}

		nanosleep(&tv, NULL);
	}

	return NULL;
}

proxy_pool_t *proxy_pool_new(int nr_workers, int nr_max)
{
	proxy_pool_t *pool;
	proxy_context_t *context;
	int i, err;

	if (nr_accepters<=0 || nr_accepters>=nr_max || nr_max<=1 || listen_sd<0) {
		mylog(L_ERR, "Invalid paremeters");
		return NULL;
	}

	pool = calloc(1, sizeof(proxy_pool_t));
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
	pool->epoll_pool = epoll_create(1);
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

int proxy_pool_delete(proxy_pool_t *pool)
{
	register int i;
	//proxy_context_t *proxy;
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
	for (curr=pool->modules->dumb->next;
			curr != pool->modules->dumb;
			curr=curr->next) {
		proxy_pool_unregister_module(pool, curr->fname);
	}
	llist_delete(pool->modules);

	// Destroy terminated_queue.	
	for (curr=pool->terminated_queue->dumb->next;
			curr != pool->terminated_queue->dumb;
			curr=curr->next) {
		proxy_context_delete(curr->ptr);
		curr->ptr = NULL;
	}
	llist_delete(pool->terminated_queue);

	// Destroy run_queue.
	for (curr=pool->run_queue->dumb->next;
			curr != pool->run_queue->dumb;
			curr=curr->next) {
		proxy_context_delete(curr->ptr);
		curr->ptr = NULL;
	}
	llist_delete(pool->run_queue);

	// Close epoll fd;
	close(pool->epoll_pool);
	pool->epoll_pool = -1;

	// Unload all modules
	proxy_pool_unload_allmodule(pool);

	// Destroy itself.
	free(pool);

	return 0;
}

int proxy_pool_load_module(proxy_pool_t *pool, const char *fname, cJSON_t *conf)
{
	module_handler_t *handle;

	handle = module_load(fname);
	if (handle==NULL) {
		return -1;
	}
	if (handle->interface->mod_initializer(conf)) {
		exit(1); // FIXME
	}
	return llist_append(pool->modules, handle);
}

int proxy_pool_unload_module(proxy_pool_t *pool, const char *fname)
{
	// TODO
	mylog(LOG_WARN, "Unloading module is not supported yet.");
	return 0;
}

static void do_unload(void *mod)
{
	module_handler_t *p=mod;

	p->interface->mod_destroier(NULL);
	module_unload(mod);
}

int proxy_pool_unload_allmodule(proxy_pool_t *pool)
{
	return llist_travel(pool->modules, do_unload);
}

cJSON *proxy_pool_serialize(proxy_pool_t *pool)
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

