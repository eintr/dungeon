/**
	\file main.c
	main()
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/resource.h>

#include "util_log.h"
#include "dungeon.h"
#include "conf.h"
#include "ds_state_dict.h"
#include "thr_monitor.h"

/** Number of cores */
int nr_cpus=0;

static int terminate=0;
dungeon_t *proxy_pool;

static char *conf_path;

/** Daemon exit. Handles most of signals. */
static void daemon_exit(int s)
{
	mylog(L_INFO, "Signal %d caught, exit now.", s); 
	//TODO: do exit
	aa_monitor_destroy();
	dungeon_delete(proxy_pool);
	server_state_destroy();
	conf_delete();
	exit(0);
}

/** Initiate signal actions */
static void signal_init(void)
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

static void rlimit_init()
{
	struct rlimit r;
	r.rlim_cur = r.rlim_max = conf_get_concurrent_max() * 5 + 1024;
	if (setrlimit(RLIMIT_NOFILE, &r) < 0) {
		mylog(L_WARNING, "set open files failed");
	}
}

int main(int argc, char **argv)
{
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

	/* set open files number */
	rlimit_init();

	if (conf_get_daemon()) {
		daemon(1, 0);
	}

	mylog_least_level(conf_get_log_level());

	chdir(conf_get_working_dir());

	signal_init();

	nr_cpus = get_nrcpu();
	if (nr_cpus<=0) {
		nr_cpus = 1;
	}
	mylog(L_INFO, "%d CPU(s) detected", nr_cpus);
	proxy_pool = dungeon_new(nr_cpus, conf_get_concurrent_max());

	while (!terminate) {
		// Process signals.
		pause();
	}

	dungeon_delete(proxy_pool);
	server_state_destroy();
	conf_delete();

	/* 
	 * Clean up
	 */

	return 0;
}

