#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "aa_log.h"
#include "proxy_pool.h"
#include "aa_conf.h"
#include "aa_state_dict.h"
#include "aa_monitor.h"

static int terminate=0;
proxy_pool_t *proxy_pool;
static int listen_sd;

static char *conf_path;

static void daemon_exit(int s)
{
	mylog(L_INFO, "Signal %d caught, exit now.", s); 
	//TODO: do exit
	aa_monitor_destroy();
	proxy_pool_delete(proxy_pool);
	server_state_destroy();
	conf_delete();
	exit(0);
}

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
			"%1$s -H\t\t"                       	\
			"Print usage and exit.\n\t"           \
			"%1$s -v\t\t"                      \
			"Print version info and exit.\n\t"    \
			"%1$s CONFIGFILE\t"                \
			"Start program with CONFIGFILE.\n\t"  \
			"%1$s\t\t"                       \
			"Start program with default configure file(%2$s)\n", a0, DEFAULT_CONFPATH);

}
static void log_init(void)
{
	mylog_reset();
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

int aa_get_options(int argc, char **argv)
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

static int socket_init()
{  
	int listen_sd;
	struct sockaddr_in localaddr;
	char *addr;
	int port;

	listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_sd<0) {
		mylog(L_ERR, "socket():%s", strerror(errno));
		return -1;
	}
	mylog(L_DEBUG, "listen socket created\n");

	int val=1;
	if (setsockopt(listen_sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val))<0) {
		mylog(L_WARNING, "Can't set SO_REUSEADDR to admin_socket.");
	}

	localaddr.sin_family = AF_INET;

	addr = conf_get_listen_addr();
	if (addr == NULL) {
		localaddr.sin_addr.s_addr = 0;
	} else {
		inet_pton(AF_INET, addr, &localaddr.sin_addr);
	}

	port = conf_get_listen_port();
	if (port == -1) {
		localaddr.sin_port = htons(DEFAULT_LISTEN_PORT);
	} else {
		localaddr.sin_port = htons(port);
	}

	if (bind(listen_sd, (void*)&localaddr, sizeof(localaddr))!=0) {
		mylog(L_ERR, "bind(): %s", strerror(errno));
		return -1;
	}

	if (listen(listen_sd, BACKLOG_NUM)<0) {
		mylog(L_ERR, "listen(): %s", strerror(errno));
		return -1;
	}

	mylog(L_DEBUG, "listen_socket is ready.");

	return listen_sd;
}

int main(int argc, char **argv)
{
	/*
	 * Parse arguments
	 */
	if (aa_get_options(argc, argv) == -1) {
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

	if (conf_get_daemon()) {
		daemon(1, 0);
	}

	mylog_least_level(conf_get_log_level());

	chdir(conf_get_working_dir());

	signal_init();

	listen_sd = socket_init();
	if (listen_sd == -1) {
		conf_delete();
		return -1;
	}

	proxy_pool = proxy_pool_new(get_nrcpu(), 1, conf_get_concurrent_max(), conf_get_concurrent_max(), listen_sd);
	mylog(L_INFO, "%d CPU(s) detected", get_nrcpu());

	aa_monitor_init();

	while (!terminate) {
		// Process signals.
		pause();
	}

	proxy_pool_delete(proxy_pool);
	server_state_destroy();
	conf_delete();

	/* 
	 * Clean up
	 */

	return 0;
}

