/** \file imp.h
	Defines imp entity.
*/

#ifndef IMP_H
#define IMP_H
/** \cond 0 */
#include <stdint.h>
/** \endcond */

#include "ds_llist.h"
#include "imp_body.h"
#include "imp_soul.h"

typedef uint32_t imp_id_t;

extern imp_id_t global_imp_id___;
#define GET_CURRENT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 0)

enum {
	IMP_NOT_RUNNING=0,
	IMP_RUNNING,
};

/** Imp struct.
	"Imp" is the name of FSM(Finite State Machine) instant in dungeon. They should be summoned(created) by rooms(modules) via room_service.h/imp_summon().
*/
typedef struct imp_st {
	imp_id_t id;		/**< Imp global uniq id */
	int in_runq;
	llist_t *requested_events;	/**< Imp request events */
	llist_t *returned_events;	/**< Returned Imp events */
	uint64_t event_mask;	/**< Imp event mask */

	imp_body_t *body;	/**< Imp body */
	imp_soul_t *soul;	/**< Imp soul. Reference to some room module. */
	void *memory;		/**< Imp local storage */
} imp_t;

struct epoll_data_st {
	uint64_t ev_mask;
	uint32_t events;
	int fd;
	imp_t *imp;
};


extern __thread imp_t *current_imp_;

/** Summon a new imp. And also append it into run_queue. Call by room module. */
imp_t *imp_summon(void *inventory, imp_soul_t *soul);

/** Dismiss an imp. Don't call this if you want to notify an imp to terminate! Use imp_kill() instead! */
int imp_dismiss(imp_t*);

/** Notify an imp to terminate. Safe. */
void imp_kill(imp_t*);

void imp_driver(imp_t*);

void imp_wake(imp_t*);

#define	IMP_ID	(current_imp_->id)
#define	IMP_TIMEDOUT	(current_imp_->event_mask & EV_MASK_TIMEOUT)
#define	IMP_IOERR	(current_imp_->event_mask & EV_MASK_IOERR)
int imp_set_ioev(int fd, uint32_t events);
int imp_set_timer(int);

int imp_cancel_timer(imp_t*);

uint64_t imp_reduct_event_mask(imp_t*);

#define	imp_yield(CODE)	do{return CODE;}while(0)

cJSON *imp_serailize(imp_t*);

#endif

