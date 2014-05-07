/** \file room_service.h
	Many useful facilities for room module
*/
#ifndef ROOM_SERVICE_H
#define ROOM_SERVICE_H

/** \cond */
#include <stdint.h>
/** \endcond */

#include "dungeon.h"
#include "imp.h"

typedef int32_t imp_id_t;

/** Summon a new imp. Call by room module. */
imp_id_t imp_summon(void *inventory, imp_soul_t *soul);

/** \todo Dismiss an imp */
int imp_dismiss(imp_id_t);

#include "modules/room_exports.h"

#endif

