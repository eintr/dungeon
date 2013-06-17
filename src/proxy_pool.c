#include <stdio.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "proxy_pool.h"
#include "proxy_context.h"
#include "aa_err.h"
#include "aa_log.h"

#define IOEV_SIZE	100
void *thr_worker(void *p)
{
	proxy_pool_t *pool=p;
	proxy_context_t *node;
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
				////mylog(L_WARNING, "run_queue is empty.");
				break;
			}
			//mylog(L_DEBUG, "[CAUTION] fetch from run queue node is %p", node);
			fprintf(stderr, "%u : [CAUTION] fetch from run queue node is %p\n", gettid(), node);
			if (is_proxy_context_timedout(node)) {
				proxy_context_timedout(node);
			} else {
				proxy_context_driver(node);
			}
		}

		num = epoll_wait(pool->epoll_pool, ioev, IOEV_SIZE, 1);
		if (num<0) {
			//mylog(L_ERR, "epoll_wait io event error");
		} else if (num==0) {
			////mylog(L_WARNING, "no io event");
		} else {
			for (i=0;i<num;++i) {
				//mylog(L_DEBUG, "[CAUTION] [G] io event ptr is %p", ioev[i].data.ptr);
				fprintf(stderr, "%u : [CAUTION] [G] io event ptr is %p\n", gettid(), ioev[i].data.ptr);
				//mylog(L_ERR, "{CAUTION} [G] event type is %d", ioev[i].events);
				fprintf(stderr, "%u : {CAUTION} [G] event type is %d\n", gettid(), ioev[i].events);
			}
			for (i=0;i<num;++i) {
				//mylog(L_WARNING, "io event happend");
				//mylog(L_ERR, "{CAUTION} io event ptr is %p", ioev[i].data.ptr);
				//mylog(L_ERR, "{CAUTION} event type is %d", ioev[i].events);
				proxy_context_driver(ioev[i].data.ptr);

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
				//mylog(L_WARNING, "terminated_queue is empty.");
				break;
			}
			fprintf(stderr, "%u : [CAUTION] fetch from terminate queue node is %p\n", gettid(), node);
			proxy_context_driver(node);
		}
		if (i==0) {
			fprintf(stderr, "%u : terminated_queue is empty.\n", gettid());
			// May be dynamically adjust the sleep interval.
		} else {
			// May be dynamically adjust the sleep interval.
		}

		nanosleep(&tv, NULL);
	}

	return NULL;
}

proxy_pool_t *proxy_pool_new(int nr_workers, int nr_accepters, int nr_max, int nr_total, int listen_sd)
{
	proxy_pool_t *pool;
	proxy_context_t *context;
	int i, err;

	if (nr_accepters<=0 || nr_accepters>=nr_max || nr_max<=1 || listen_sd<0) {
		//log error
		return NULL;
	}

	pool = calloc(1, sizeof(proxy_pool_t));
	if (pool == NULL) {
		//mylog(L_ERR, "not enough memory");
		return NULL;
	}

	pool->nr_max = nr_max;
	pool->nr_busy = 0;
	pool->run_queue = llist_new(nr_total);
	//newnode->iowait_queue_ht = hasht_new(NULL, nr_total);
	pool->terminated_queue = llist_new(nr_total);
	pool->original_listen_sd = listen_sd;
	//pool->epoll_accept = epoll_create(1);
	pool->epoll_pool = epoll_create(1);
	pool->worker = malloc(sizeof(pthread_t)*nr_workers);
	if (pool->worker==NULL) {
		//mylog(L_ERR, "not enough memory for worker");
		exit(1);
	}
	for (i=0;i<nr_workers;++i) {
		err = pthread_create(pool->worker+i, NULL, thr_worker, pool);
		if (err) {
			//mylog(L_ERR, "create worker thread failed");
			break;
		}
	}
	if (i==0) {
		//mylog(L_ERR, "create worker thread failed");
		abort();
	} else {
		//mylog(L_INFO, "create %d worker threads", i);
	}

	err = pthread_create(&pool->maintainer, NULL, thr_maintainer, pool);
	if (err) {
		//mylog(L_ERR, "create maintainer thread failed");
	}

	context = proxy_context_new_accepter(pool);
	proxy_context_put_epollfd(context);

	return pool;
}

int proxy_pool_delete(proxy_pool_t *pool)
{
	register int i;
	//proxy_context_t *proxy;
	llist_node_t *curr;

	if (pool->worker_quit == 0) {
		pool->worker_quit = 1;
		for (i=0;i<pool->nr_workers;++i) {
			pthread_join(pool->worker[i], NULL);
		}
	}

	if (pool->maintainer_quit == 0) {
		pool->maintainer_quit = 1;
		pthread_join(pool->maintainer, NULL);
	}

	for (curr=pool->terminated_queue->dumb->next;
			curr != pool->terminated_queue->dumb;
			curr=curr->next) {
		proxy_context_delete(curr->ptr);
		curr->ptr = NULL;
	}
	llist_delete(pool->terminated_queue);

	for (curr=pool->run_queue->dumb->next;
			curr != pool->run_queue->dumb;
			curr=curr->next) {
		proxy_context_delete(curr->ptr);
		curr->ptr = NULL;
	}
	llist_delete(pool->run_queue);

	free(pool);

	return 0;
}

cJSON *proxy_pool_info(proxy_pool_t *pool)
{
	cJSON *result;

	result = cJSON_CreateObject();

	//cJSON_AddItemToObject(result, "MinIdleProxy", cJSON_CreateNumber(pool->nr_minidle));
	cJSON_AddItemToObject(result, "MaxIdleProxy", cJSON_CreateNumber(pool->nr_max));
	cJSON_AddItemToObject(result, "MaxProxy", cJSON_CreateNumber(pool->nr_total));
	cJSON_AddItemToObject(result, "NumIdle", cJSON_CreateNumber(pool->nr_idle));
	cJSON_AddItemToObject(result, "NumBusy", cJSON_CreateNumber(pool->nr_busy));
	cJSON_AddItemToObject(result, "RunQueue", llist_info_json(pool->run_queue));
	cJSON_AddItemToObject(result, "TerminatedQueue", llist_info_json(pool->terminated_queue));
	cJSON_AddItemToObject(result, "NumWorkerThread",cJSON_CreateNumber(pool->nr_workers));

	return result;
}

