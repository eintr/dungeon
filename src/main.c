/**
	\file main.c
	main()
*/

/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/resource.h>
/** \endcond */

#include "util_log.h"
#include "dungeon.h"
#include "conf.h"
#include "ds_state_dict.h"
#include "thr_monitor.h"

/** Number of cores */
int nr_cpus=0;

static int terminate=0;

static char *conf_path;

/** Daemon exit. Handles most of signals. */
static void daemon_exit(int s)
{
	if (s>0) {
		mylog(L_INFO, "Signal %d caught, exit now.", s); 
	} else {
		mylog(L_INFO, "Synchronized exit."); 
	}
	//TODO: do exit
	dungeon_delete();
	conf_delete();
	exit(0);
}

/** Initiate signal actions */
static void signal_init(void(*handler)(int))
{
	struct sigaction sa; 

	sa.sa_handler = daemon_exit;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGTERM);
	sigaddset(&sa.sa_mask, SIGQUIT);
	sigaddset(&sa.sa_mask, SIGINT);
	sa.sa_flags = 0;

	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	sa.sa_handler = SIG_IGN;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGPIPE, &sa, NULL);
}

static int get_nrcpu(void)
{   
	cpu_set_t set;
	int i, num = 0;

	if (sched_getaffinity(0, sizeof(set), &set)<0) {
		mylog(L_ERR, "sched_getaffinity(): %m");
		return 1;
	}

	for(i = 0; i < sizeof(set) * 8; i++){
		num += CPU_ISSET(i, &set);
	}   
	return num;
} 

static void usage(const char *a0)
{   
	fprintf(stderr, "Usage: \n\t"				\
			"%1$s -h\t\t"                       	\
			"Print usage and exit.\n\t"           \
			"%1$s -v\t\t"                      \
			"Print version info and exit.\n\t"    \
			"%1$s -c CONFIGFILE\t"                \
			"Start program with CONFIGFILE.\n\t"  \
			"%1$s\t\t"                       \
			"Start program with default configure file(%2$s)\n", a0, DEFAULT_CONFPATH);

}
static void log_init(void)
{
	mylog_reset();
	mylog_set_target(LOGTARGET_STDERR);
	mylog_set_target(LOGTARGET_SYSLOG, APPNAME, LOG_DAEMON);
	if (conf_get_log_level() == L_DEBUG) {
		mylog_set_target(LOGTARGET_STDERR);
	} else {
		//TODO:
		//mylog_set_target(LOGTARGET_STDERR);
	}
}

static void version(void)
{   
	fprintf(stdout, "%s\n", APPVERSION);
}

static int dungeon_get_options(int argc, char **argv)
{
	int c;

	while (-1 != (c = getopt(argc, argv,
					"c:" /* configure file */
					"v" /* version */
					"h" /* help */
					))) {
		switch (c) {
			case 'c': /* configure file path */
				conf_path = optarg;
				break;
			case 'v': /* show version */
				version();
				break;
			case 'h': /* usage */
				usage(argv[0]);
				exit(EXIT_SUCCESS);
			default:
				fprintf(stderr, "Invalid arguments");
				return -1;
		}
	}

	return 0;
}

static rlim_t rlimit_try(rlim_t left, rlim_t right)
{
	struct rlimit r;

	if (left>right) {
		return rlimit_try(right, left);
	}
	if (left==right || left==right-1) {
		return left;
	}
	r.rlim_cur = right;
	r.rlim_max = r.rlim_cur;
	if (setrlimit(RLIMIT_NOFILE, &r)==0) {
		return right;
	}
	r.rlim_cur = (right-left)/2+left;
	r.rlim_max = r.rlim_cur;
	if (setrlimit(RLIMIT_NOFILE, &r)==0) {
		return rlimit_try(r.rlim_cur, right);
	} else {
		return rlimit_try(left, r.rlim_cur);
	}
}

static void rlimit_init()
{
	mylog(L_DEBUG, "Try to set ulimit -n =%d", conf_get_concurrent_max() * 5 + 1024);
	mylog(L_INFO, "ulimit -n => %d", rlimit_try(1024, conf_get_concurrent_max() * 5 + 1024));
}

int main(int argc, char **argv)
{
	pid_t pid;

	/*
	 * Parse arguments
	 */
	if (dungeon_get_options(argc, argv) == -1) {
		return -1;
	}

	/*
	 * Parse config file
	 */
	if (conf_path == NULL) {
		conf_path = DEFAULT_CONFPATH;
	}

	if (conf_new(conf_path) == -1) {
		fprintf(stderr, "cannot find configure file: [%s], exit.\n", conf_path);
		return -1;
	}

	/*
	 * Init
	 */
	log_init();
	mylog_least_level(conf_get_log_level());

	if (conf_get_daemon()) {
		daemon(1, 0);
		strncpy(argv[0], "Dungeon Keeper", strlen(argv[0]));
		signal_init(daemon_exit);
		while(1) {
			pid = fork();
			if (pid!=0) {
				while (waitpid(pid, NULL, 0)<0) {
					if (errno==EINTR) {
						mylog(L_DEBUG, "Signal interrupted waitpid(), again.");
						continue;
					}
					mylog(L_ERR, "Dungeon heart broken! Create again!");
				}
			} else {
				mylog(L_INFO, "Initiate dungeon heart.");
				strncpy(argv[0], "Dungeon Heart", strlen(argv[0]));
				goto server_start;
			}
		}
	}

server_start:

	chdir(conf_get_working_dir());

	/* set open files number */
	rlimit_init();

	signal_init(daemon_exit);

	nr_cpus = get_nrcpu();
	if (nr_cpus<=0) {
		nr_cpus = 1;
	}
	mylog(L_INFO, "%d CPU(s) detected", nr_cpus);
	if (dungeon_init(conf_get_workers(nr_cpus), conf_get_concurrent_max())!=0) {
		mylog(L_ERR, "Can't create dungeon heart!");
		abort();
	}

	while (!terminate) {
		// Process signals.
		pause();
	}

	daemon_exit(0);
	return 0;
}

