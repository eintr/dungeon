/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
/** \endcond */

#include "util_log.h"
#include "util_atomic.h"

#include "room_service.h"
//#include "imp.h"
//#include "dungeon.h"

uint32_t imp_summon(dungeon_t *dungeon, void *memory, imp_soul_t *soul)
{
	imp_t *imp = NULL;

	imp = imp_new(soul);
	if (imp) {
		imp->memory = memory;

		atomic_increase(&dungeon->nr_total);

		imp_set_run(dungeon, imp);
	}
	return imp->id;
}

