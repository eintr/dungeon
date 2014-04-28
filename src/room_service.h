#ifndef ROOM_SERVICE_H
#define ROOM_SERVICE_H

#include <stdint.h>

#include "dungeon.h"
#include "imp_soul.h"

// Used by module
uint32_t imp_summon(dungeon_t *d, void *context_spec_data, imp_soul_t *soul);

int imp_dismiss(dungeon_t *d, uint32_t);

#endif

