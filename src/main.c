#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sched.h>

#include "mylog.h"
#include "proxy_pool.h"

static int terminate=0;
static proxy_pool_t *proxy_pool;
static int listen_sd;

static void daemon_exit(int s)
{
	proxy_pool_delete(proxy_pool);

	// Clean up

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
	int count=0;

	if (sched_getaffinity(0, &set, sizeof(set))<0) {
		mylog(L_ERR, "sched_getaffinity(): %s", strerror(errno));
		return 1;
	}
	return CPU_COUNT(&set);
}

static void usage(const char *a0)
{   
	fprintf(stderr, "Usage: \n\t				\
			%s -H\t\t						\
			Print usage and exit.\n\t			\
			thralld -v\t\t						\
			Print version info and exit.\n\t	\
			thralld CONFIGFILE\t				\
			Start program with CONFIGFILE.\n\t	\
			thralld\t\t\t						\
			Startprogram with default configure file(%s)\n", a0, "DEFAULT_CONFPATH");
}

static void version(void)
{   
	fprintf(stderr, "%s\n", "APPVERSION");
}

int aa_get_options(int argc, char **argv)
{
	int c;

	while (-1 != (c = getopt(argc, argv,
					"c:"
					"h:"))) {
		switch (c) {
			case 'c': /* configure file path */
				conf_path = optarg;
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

int main(int argc, char **argv)
{

	// Parse config file

	if (aa_get_options(argc, argv) == -1) {
		return -1;
	}

	// Parse arguments

	// Init
	log_init();
	listen_sd = socket_init();

	proxy_pool = proxy_pool_new(get_nrcpu(), 1, conf_get_concurrent_max(NULL), listen_sd);
	if (0) {
	}

	while (!terminate) {
		pause();
		// Process signals.
	}

	return 0;
}

