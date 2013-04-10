#include <stdio.h>
#include <stdlib.h>

#include ""

static int terminate=0;

int main(int argc, char **argv)
{
	proxy_pool_t *proxy_pool;
	// Parse config file
	
	// Parse arguments

	// Init

	proxy_pool = proxy_pool_new();
	if () {
	}

	while (!terminate) {
		pause();
		// Process signals.
	}

	proxy_pool_cancel(proxy_pool);

	proxy_pool_join(proxy_pool);

	proxy_pool_delete(proxy_pool);

	// Clean up

	return 0;
}

