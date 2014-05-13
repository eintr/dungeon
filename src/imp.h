/** \file imp.h
	Defines imp entity.
*/

#ifndef IMP_H
#define IMP_H
/** \cond 0 */
#include <stdint.h>
/** \endcond */

#include "imp_body.h"
#include "imp_soul.h"

struct dungeon_st;

typedef uint32_t imp_id_t;

extern imp_id_t global_imp_id___;
#define GET_CURRENT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 0)

/** Imp struct.
	"Imp" is the name of FSM(Finite State Machine) instant in dungeon. They should be summoned(created) by rooms(modules) via room_service.h/imp_summon().
*/
typedef struct imp_st {
	imp_id_t id;		/**< Imp global uniq id */
	int kill_mark;		/**< Imp should quit */

	imp_body_t *body;	/**< Imp body */
	imp_soul_t *soul;	/**< Imp soul. Reference to some room module. */
	void *memory;		/**< Imp local storage */
} imp_t;

/** Summon a new imp. And also append it into run_queue. Call by room module. */
imp_t *imp_summon(void *inventory, imp_soul_t *soul);

/** \todo Dismiss an imp */
int imp_dismiss(imp_t*);

void imp_driver(imp_t*);

int imp_set_ioev(imp_t*, int fd, struct epoll_event*);
int imp_get_ioev(imp_t*, struct epoll_event*);

int imp_set_timer(imp_t*, struct itimerval*);

cJSON *imp_serailize(imp_t*);

#endif

