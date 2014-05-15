/** \file room_service.h
	Many useful facilities for room module
*/
#ifndef ROOM_SERVICE_H
#define ROOM_SERVICE_H

/** \cond */
#include <stdint.h>
#include "cJSON.h"
/** \endcond */

#include "dungeon.h"
#include "imp.h"
#include "util_log.h"
#include "util_conn_tcp.h"

int imp_settimeout(imp_t*, struct timeval*);
int imp_istimedout(imp_t*);

#include "modules/room_exports.h"

#endif

