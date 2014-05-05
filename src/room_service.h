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

/** Summon a new imp. Used by room module. */
uint32_t imp_summon(dungeon_t *d, void *inventory, imp_soul_t *soul);

/** \todo Dismiss an imp */
int imp_dismiss(dungeon_t *d, uint32_t);

#endif

