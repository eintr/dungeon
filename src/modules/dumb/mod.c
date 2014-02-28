#include <stdio.h>
#include <dungeon.h>
#include <room_template.h>
#include <room_service.h>

static struct {
	uint16_t listen_port;
	struct timeval rtimeo, stimeo;
} config;

static dungeon_t *dungeon=NULL;
static int listen_sd;
static imp_soul_t soul;

static int mod_init(void *p, cJSON *conf)
{
	uint32_t id;

	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	dungeon = p;

	id = imp_summon(dungeon, NULL, &soul);
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

static int fsm_delete(void *unused)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return 0;
}

static enum enum_driver_retcode fsm_driver(void *unused)
{
	static count =10;

	fprintf(stderr, "%s is running.\n", __FUNCTION__);

	if (count==0) {
		return TO_TERM;
	}
	--count;
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

