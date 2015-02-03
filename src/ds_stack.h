#ifndef DS_STACK_H
#define DS_STACK_H

#include <stdint.h>

typedef void ds_stack_t;

ds_stack_t *stack_new(int volume);

int stack_delete(ds_stack_t *);

int stack_push(ds_stack_t *, void*);

void *stack_pop(ds_stack_t *);

size_t stack_count(ds_stack_t *);

#endif

