/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <cJSON.h>
/** \endcond */

#include <room_service.h>
#include <util_log.h>

typedef struct {
	int count;
} memory_t ;

static int listen_sd;
static imp_soul_t soul;

static int mod_init(cJSON *conf)
{
	uint32_t id;
	memory_t *mem;

	fprintf(stderr, "%s is running.\n", __FUNCTION__);

	mem = malloc(sizeof(*mem));
	mem->count = 5;

	id = imp_summon(mem, &soul);
	if (id==0) {
		fprintf(stderr, "imp_summon() Failed!\n");
		return 0;
	}
	fprintf(stderr, "imp[%d] summoned\n", id);
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

static int fsm_delete(void *m)
{
	memory_t *mem=m;

	mylog(L_DEBUG, "%s is running, free memory.\n", __FUNCTION__);
	free(mem);
	return 0;
}

static enum enum_driver_retcode fsm_driver(void *m)
{
	static count =10;
	memory_t *mem=m;

	fprintf(stderr, "%s is running (%d).\n", __FUNCTION__, mem->count);

	if (mem->count==0) {
		return TO_TERM;
	}
	--mem->count;
	return TO_RUN;
}

static void *fsm_serialize(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

static imp_soul_t soul = {
	.fsm_new = fsm_new,
	.fsm_delete = fsm_delete,
	.fsm_driver = fsm_driver,
	.fsm_serialize = fsm_serialize,
};

module_interface_t MODULE_INTERFACE_SYMB = 
{
	.mod_initializer = mod_init,
	.mod_destroier = mod_destroy,
	.mod_maintainer = mod_maint,
	.mod_serialize = mod_serialize,
};

