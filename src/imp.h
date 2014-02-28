#ifndef IMP_H
#define IMP_H

#include <stdint.h>

#include "imp_body.h"
#include "imp_soul.h"

struct dungeon_st;

extern uint32_t global_context_id___;
#define GET_CURRENT_IMP_ID __sync_fetch_and_add(&global_context_id___, 0)

typedef struct imp_st {
	uint32_t id;

	imp_body_t *body;
	imp_soul_t *soul;
	void *memory;
} imp_t;

imp_t *imp_new(imp_soul_t *soul);
int imp_delete(imp_t *);

void imp_set_run(struct dungeon_st *, imp_t *);
void imp_set_iowait(struct dungeon_st *, int fd, imp_t *);
void imp_set_term(struct dungeon_st *, imp_t *);

#endif

