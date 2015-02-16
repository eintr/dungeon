#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <signal.h>
#include <time.h>

#include "dungeon.h"
#include "thr_sched.h"
#include "util_syscall.h"
#include "util_err.h"
#include "util_log.h"
#include "util_atomic.h"
#include "imp.h"
#include "ds_simple_queue.h"
#include "util_misc.h"

struct thr_env_st {
	pthread_t tid;
	int cpubind;

	int id;
	simqueue_t *thr_run_queue;
	olist_t *timeout_index;
	int epoll_fd;					/**< Event engine. This is an epoll file descriptor */
};

static struct thr_env_st *info_thr;
static int num_thr;
static volatile int worker_quit=0;

static __thread volatile simqueue_t *thr_run_queue = NULL;
static __thread volatile olist_t *thr_timeout_index = NULL;
static __thread volatile int thr_epoll_fd=-1;

const int IOEV_SIZE=10240;

/** Worker thread function */
static void *thr_worker(void *p)
{
	struct thr_env_st *thr_env = p;
	intptr_t worker_id = thr_env->id;
	imp_t *imp, *head;
	sigset_t allsig;
	struct epoll_event ioev[IOEV_SIZE], null_ev;
	uint64_t now;
	int i, timeout, num, count;

	/** Init TLS thread scope vars */
	thr_epoll_fd = thr_env->epoll_fd;
	thr_timeout_index = thr_env->timeout_index;
	thr_run_queue = thr_env->thr_run_queue;

	/** Init thread signal and cancel states */
	sigfillset(&allsig);
	pthread_sigmask(SIG_BLOCK, &allsig, NULL);
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);

	while (!worker_quit) {
		/** Process imps in runqueue first. */
		while (	simqueue_dequeue(thr_run_queue, (void**)&imp)==0 ||
				queue_dequeue_nb(dungeon_heart->balance_queue, (void**)&imp)==0) {
/*
		while (1) {
			if (simqueue_dequeue(thr_run_queue, (void**)&imp)!=0) {
//				fprintf(stderr, "sched[%d]: Local queue is empty.\n", worker_id);
				if (queue_dequeue_nb(dungeon_heart->balance_queue, (void**)&imp)!=0) {
//					fprintf(stderr, "sched[%d]: Global queue is empty, too. Continue to IO process.\n", worker_id);
					break;
				} else {
					fprintf(stderr, "sched[%d]: Got imp%p from global queue.\n", worker_id, imp);
				}
			} else {
				fprintf(stderr, "sched[%d]: Got imp%p from local queue.\n", worker_id, imp);
			}
*/
			if (imp==NULL) {
				fprintf(stderr, "Got a NULL imp pointer from queue, There must be a BUG!\n");
				continue;
			}
			if (imp->memory==NULL) {
				fprintf(stderr, "thr_worker: !! Got imp[%d] with memory==NULL!\n", imp->id);
			}
			atomic_increase(&dungeon_heart->nr_busy_workers);
			current_imp_ = imp;
			imp_driver(imp);
			atomic_decrease(&dungeon_heart->nr_busy_workers);
		}

		now = systimestamp_ms();

		/** Check is there are expired imps */
		head = olist_peek_head(thr_timeout_index);
		if (head==NULL) {
            timeout=999;
		} else{
            timeout = head->timeout_ms - now;
            if (timeout<0) {
fprintf(stderr, "thr_ioevent: imp[%d] has timedout -> %llu-%llu = %d.\n", head->id, head->timeout_ms, now, timeout);
                timeout=0;
            } else if (timeout>999) {
				timeout=999;
			}
        }

		/** Call epoll_wait() if there are no expired imps. */
        if (timeout>0) {
            num = epoll_wait(thr_epoll_fd, ioev, IOEV_SIZE, timeout);
            if (num<0 && errno!=EINTR) {
                mylog(L_ERR, "thr_ioevent(): epoll_wait(): %m");
            }
        } else {
            num = 0;
        }

		/** Process expired imps */
        count=0;
        while (1) {
            imp = olist_fetch_head(thr_timeout_index);
            if (imp==NULL) {
                break;
            }
            if (imp->timeout_ms < now) { /* Timed out */
fprintf(stderr, "imp[%u] timed out(%llu < %llu).\n", imp->id, imp->timeout_ms, now);
                imp->ioev_revents = EV_MASK_TIMEOUT;
                atomic_decrease(&dungeon_heart->nr_waitio);
                epoll_ctl(thr_epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &null_ev);
                queue_enqueue(thr_run_queue, imp);
                ++count;
            } else {
                if (olist_add_entry(thr_timeout_index, imp)!=0) {    /* Feed non-timedout imp back. */
fprintf(stderr, "Failed to feed imp[%d] back to timeout_index.\n");
abort();
                }
                break;
            }
        }
//fprintf(stderr, "Waked %d expired imps\n", count);

		/** Process imps that waken up by IO events */
		for (i=0;i<num;++i) {
			imp = ioev[i].data.ptr;
			imp->ioev_revents = ioev[i].events;
			if (olist_remove_entry(thr_timeout_index, imp)!=0) {
				abort();
			}
			atomic_decrease(&dungeon_heart->nr_waitio);
			if (imp->memory==NULL) {
				fprintf(stderr, "thr_ioevent: !! Got imp[%d] with memory==NULL!\n", imp->id);
				abort();
			} else {
				simqueue_enqueue(thr_run_queue, imp);
			}
		}
//fprintf(stderr, "Waked %d imps by IO events.\n", num);
	}

	olist_destroy(thr_timeout_index);
	simqueue_delete(thr_run_queue);
	pthread_exit(NULL);
}

int thr_worker_create(int num, cpu_set_t *cpuset)
{
	int err;
	int c;
	cpu_set_t thr_cpuset;
	pthread_attr_t attr;

	info_thr = malloc(num*sizeof(struct thr_env_st));
	if (info_thr==NULL) {
		mylog(L_WARNING, "malloc(): %m.");
		return -1;
	}

	c=0;
	for (num_thr=0; num_thr<num; ++num_thr) {
		for (;c<CPU_SETSIZE && !CPU_ISSET(c, &dungeon_heart->process_cpuset); ++c);
		if (CPU_ISSET(c, &dungeon_heart->process_cpuset)) {
			info_thr[num_thr].cpubind = c;
		}
		++c;
		if (c==CPU_SETSIZE) {
			c = CPU_SETSIZE-1;
		}
	}
	for (num_thr=0; num_thr<num; ++num_thr) {
		pthread_attr_init(&attr);
		CPU_ZERO(&thr_cpuset);
		CPU_SET(info_thr[num_thr].cpubind, &thr_cpuset);
//		pthread_attr_setaffinity_np(&attr, sizeof(thr_cpuset), &thr_cpuset);

		info_thr[num_thr].id = num_thr;
		info_thr[num_thr].thr_run_queue = simqueue_new(1000000);
		info_thr[num_thr].timeout_index = olist_new(1000000, imp_timeout_cmp);
		info_thr[num_thr].epoll_fd = epoll_create(1);

		err = pthread_create(&info_thr[num_thr].tid, &attr, thr_worker, (void*)(info_thr+num_thr));
		if (err) {
			break;
		}
		pthread_attr_destroy(&attr);
	}
	sleep(1);	// FIXME! Use a barrier!
	return num_thr;
}

int thr_worker_destroy(void)
{
	int i;

	worker_quit = 1;
	for (i=0;i<num_thr;++i) {
		pthread_join(info_thr[i].tid, NULL);
	}
	return 0;
}

static int P_migrating(simqueue_t *queue)
{
	// TODO:
	return 0;
}

int sched_run(imp_t *imp)
{
	if (imp==NULL) {	// DEBUG
		fprintf(stderr, "!!! Trying to wakeup a NULL imp!!\n");
		abort();
	}
	if (current_imp_==NULL) {	// If we are not within any a thr_sched scope.
		fprintf(stderr, "Put imp %p to balance_queue\n", imp);
		queue_enqueue(dungeon_heart->balance_queue, (void*)imp);
	} else if (P_migrating(thr_run_queue)) {
		olist_remove_entry(thr_timeout_index, imp);
		queue_enqueue(dungeon_heart->balance_queue, (void*)imp);
	} else {
		simqueue_enqueue(thr_run_queue, (void*)imp);
	}
	return 0;
}

int sched_sleep(imp_t *imp)
{
	struct epoll_event ev;

	if (imp==NULL) {	// DEBUG
		fprintf(stderr, "!!! Trying to sleep a NULL imp!!\n");
		abort();
	}
	if (olist_add_entry(thr_timeout_index, imp)!=0) {
		fprintf(stderr, "Failed to insert imp[%d] into timeout_index.\n", imp->id);
	}
	ev.data.ptr = imp;
	ev.events = imp->ioev_events | EPOLLRDHUP | EPOLLONESHOT;
	if (epoll_ctl(thr_epoll_fd, EPOLL_CTL_MOD, imp->ioev_fd, &ev)) {
		if (epoll_ctl(thr_epoll_fd, EPOLL_CTL_ADD, imp->ioev_fd, &ev)) {
			mylog(L_ERR, "Imp[%d]: Can't register imp to dungeon_heart->epoll_fd, epoll_ctl(): %m, cancel it!\n", imp->id);
			abort();
		}
	}
	atomic_increase(&dungeon_heart->nr_waitio);
	return 0;
}

int sched_term(imp_t *imp)
{
	if (imp==NULL) {	// DEBUG
		fprintf(stderr, "!!! Trying to terminate a NULL imp!!\n");
		abort();
	}
	olist_remove_entry(thr_timeout_index, imp);
	imp_rip(imp);
	return 0;
}

cJSON *thr_worker_serialize(void)
{
	int i;
	cJSON *result, *item;

	result = cJSON_CreateArray();
	for (i=0;i<num_thr;++i) {
		item = cJSON_CreateObject();
		cJSON_AddNumberToObject(item, "WorkerID", i);
		cJSON_AddNumberToObject(item, "CPUBind", info_thr[i].cpubind);
		cJSON_AddItemToArray(result, item);
	}
	return result;
}

