/** \file imp.h
	Defines imp entity.
*/

#ifndef IMP_H
#define IMP_H
/** \cond 0 */
#include <stdint.h>
#include "cJSON.h"
/** \endcond */

#include "imp_soul.h"
#include "util_mutex.h"

typedef uint32_t imp_id_t;

extern imp_id_t global_imp_id___;
#define GET_CURRENT_IMP_ID __sync_fetch_and_add(&global_imp_id___, 0)

#define EV_MASK_TIMEOUT		0x80000000UL
#define EV_MASK_KILL		0x40000000UL
#define EV_MASK_GLOBAL_KILL	0x20000000UL
#define EV_MASK_STAT		0x10000000UL

/** Imp struct.
	"Imp" is the name of FSM(Finite State Machine) instant in dungeon. They should be summoned(created) by rooms(modules) via room_service.h/imp_summon().
*/
typedef struct imp_st {
	imp_id_t id;		/**< Imp global uniq id */

	int ioev_fd;
	uint64_t timeout_ms;
	uint32_t ioev_events;	/**< Imp requested events */
	uint32_t ioev_revents;	/**< Events returned by epoll_wait() */

	imp_soul_t *soul;	/**< Imp soul. Reference to some room module. */
	void *memory;		/**< Imp local storage */
} imp_t;

#define	TIMEOUT_MAX INT32_MAX;

extern __thread volatile imp_t *current_imp_;

/** Summon a new imp. And also append it into run_queue. Call by room module. */
imp_t *imp_summon(void *inventory, imp_soul_t *soul);

/** Rest in peace. Don't call this if you want to notify an imp to terminate! Use imp_kill() instead! */
int imp_rip(imp_t*);

int imp_timeout_cmp(void*, void*);
int imp_ioevfd_cmp(void*, void*);

/** Notify an imp to terminate. Safe. */
void imp_kill(imp_t*);

void imp_driver(imp_t*);

void imp_wake(imp_t*);

#define	IMP_ID	(current_imp_->id)
#define	IMP_TIMEDOUT	(current_imp_->ioev_revents & EV_MASK_TIMEOUT)
int imp_set_ioev(int fd, uint32_t events);
int imp_set_timer(int);

cJSON *imp_serailize(imp_t*);

#endif

