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

extern uint32_t global_imp_id___;
#define GET_CURRENT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 0)

/** Imp struct.
	"Imp" is the name of FSM(Finite State Machine) instant in dungeon. They should be summoned(created) by rooms(modules) via room_service.h/imp_summon().
*/
typedef struct imp_st {
	uint32_t id;		/**< Imp global uniq id */

	imp_body_t *body;	/**< Imp body */
	imp_soul_t *soul;	/**< Imp soul. Reference to some room module. */
	void *memory;		/**< Imp local storage */
} imp_t;

/** Create a new imp with specific soul */
imp_t *imp_new(imp_soul_t *soul);

/** Delete an imp */
int imp_delete(imp_t *);

void imp_set_run(struct dungeon_st *, imp_t *);
void imp_set_iowait(struct dungeon_st *, int fd, imp_t *);
void imp_set_term(struct dungeon_st *, imp_t *);

#endif

