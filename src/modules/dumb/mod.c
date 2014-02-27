#include <stdio.h>
#include <proxy_pool.h>
#include <aa_module_interface.h>
#include <aa_module_utils.h>

static struct {
	uint16_t listen_port;
	struct timeval rtimeo, stimeo;
} config;

static imp_pool_t *pool=NULL;
static int listen_sd;

static int mod_init(void *p, cJSON *conf)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	pool = p;
	listen_sd = socket();
	bind();
	listen();
	return 0;
}

static int mod_destroy(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	close(listen_sd);
	return 0;
}

static void mod_maint(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
}

static cJSON *mod_serialize(void *ptr)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

static void *fsm_new(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

static int fsm_delete(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return 0;
}

static enum enum_driver_retcode fsm_driver(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return TO_RUN;
}

static void *fsm_serialize(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

module_interface_t MODULE_INTERFACE_SYMB = 
{
	.mod_initializer = mod_init,
	.mod_destroier = mod_destroy,
	.mod_maintainer = mod_maint,
	.mod_serialize = mod_serialize,
	.fsm_new = fsm_new,
	.fsm_delete = fsm_delete,
	.fsm_driver = fsm_driver,
	.fsm_serialize = fsm_serialize,
};

