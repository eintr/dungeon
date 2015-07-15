#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "util_mutex.h"

#include "ds_stack.h"

struct stack_st {
	size_t sp, memsize;
	int volume;
	mutex_t mut;
	void *data[0];
};

ds_stack_t *stack_new(int volume)
{
	struct stack_st *mem;

	mem = malloc(sizeof(struct stack_st) + sizeof(void *)*volume);
	mem->memsize = sizeof(void *)*volume;
	mem->sp = 0;
	mem->volume = volume;
	mem->mut = MUTEX_UNLOCKED;
	return mem;
}

int stack_delete(ds_stack_t *p)
{
	struct stack_st *stack = p;

	free(stack);
	return 0;
}

int stack_push(ds_stack_t *p, void * data)
{
	struct stack_st *stack = p;

	mutex_lock(&stack->mut);
	if (stack->sp>=stack->volume) {
		mutex_unlock(&stack->mut);
		return -EAGAIN;
	}
	stack->data[stack->sp] = data;
	stack->sp++;
	mutex_unlock(&stack->mut);
	return 0;
}


void * stack_pop(ds_stack_t *p)
{
	struct stack_st *stack = p;
	void * ret;

	mutex_lock(&stack->mut);
	if (stack->sp==0) {
		mutex_unlock(&stack->mut);
		return NULL;
	}
	stack->sp--;
	ret = stack->data[stack->sp];
	mutex_unlock(&stack->mut);
	return ret;
}

size_t stack_count(ds_stack_t *p)
{
	struct stack_st *stack = p;

	return stack->sp;
}

