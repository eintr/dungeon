#include <stdio.h>
#include <sys/epoll.h>

#include "proxy_pool.h"

#define IOEV_SIZE	1

void *thr_worker(void *p)
{
	proxy_pool_t *pool=p;
	proxt_context_t *node;
	int err, num;
	struct epoll_event ioev[IOEV_SIZE];

	while (1) {
		while (1) {
			err = llist_fetch_head_nb(pool->run_queue, *node);
			if (err==AA_EAGAIN) {
				mylog("run_queue is empty.");
				break;
			}
			proxy_context_driver(node);
		}
		num = epoll_wait(epoll_fd, ioev, IOEV_SIZE, -1);
		if (num<0) {
			mylog();
		} else if (num==0) {
			mylog();
		} else {
			for (i=0;i<num;++i) {
				proxy_context_driver(ioev[i].data.ptr);
			}
		}
	}
}

void *thr_maintainer(void *p)
{
	proxy_context_t *proxy;
	while (1) {
		pthread_mutex_lock();
		if (nr_idle<nr_minidle) {
			// Add some worker
		} else if (nr_idle>nr_maxidle) {
			// Remove some worker
		}
		pthread_mutex_unlock();

		while (llist_fetch_head_nb(terminated_queue, &proxy))==0) {
			// Destroy thos proxy
		}

		sleep(1);
	}
}

proxy_pool_t *proxy_pool_new(int nr_workers, int nr_minidle, int nr_maxidle, int nr_total, int listensd)
{
	proxy_pool_t *newnode;

	if (nr_minidle>nr_maxidle || nr_minidle>nr_total || nr_maxidle>nr_total || nr_total<1 || listensd<0) {
		return AA_EINVAL;
	}

	newnode = malloc(sizeof(*newnode));
	if (newnode==NULL) {
		mylog();
		return AA_ENOMEM;
	}

	newnode->nr_minidle = nr_minidle;
	newnode->nr_maxidle = nr_maxidle;
	newnode->nr_total = nr_total;
	newnode->nr_busy = 0;
	newnode->run_queue = llist_new(nr_total);
	newnode->iowait_queue_ht = hasht_new(NULL, nr_total);
	newnode->terminated_queue = llist_new(nr_total);
	newnode->original_listen_sd = listen_sd;
	newnode->worker = malloc(sizeof(pthread_t)*nr_workers);
	if (newnode->worker==NULL) {
		mylog();
		exit(1);
	}
	for (i=0;i<nr_workers;++i) {
		err = pthread_create(newnode->worker+i, NULL, thr_worker, newnode);
		if (err) {
			mylog();
			break;
		}
	}
	if (i==0) {
		mylog();
		abort();
	} else {
		mylog();
	}

	err = pthread_create(&newnode->maintainer, NULL, thr_maintainer, newnode);
	if (err) {
		mylog();
		break;
	}

	return newnode;
}


