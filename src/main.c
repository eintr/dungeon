#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>

#include "mylog.h"
#include "proxy_pool.h"

static int terminate=0;


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

static void usage(void)
{   
	fprintf(stderr, "Usage: \n\t
			thralld -H\t\t
			Print usage and exit.\n\t
			thralld -v\t\t
			Print version info and exit.\n\t
			thralld CONFIGFILE\t
			Start program with CONFIGFILE.\n\t
			thralld\t\t\t
			Startprogram with default configure file(%s)\n", DEFAULT_CONFPATH);
}

static void version(void)
{   
	fprintf(stderr, "%s\n", APPVERSION);
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
				usage();
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
	proxy_pool_t *proxy_pool;
	// Parse config file

	if (aa_get_options(argc, argv) == -1) {
		return -1;
	}

	// Parse arguments

	// Init
	log_init();

	proxy_pool = proxy_pool_new();

	if () {
	}

	while (!terminate) {
		pause();
		// Process signals.
	}

	proxy_pool_delete(proxy_pool);

	// Clean up

	return 0;
}

