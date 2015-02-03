#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>

#include "ds_stack.h"

struct stack_st {
	size_t sp, memsize;
	int volume;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	intptr_t data[0];
};

ds_stack_t *stack_new(int volume)
{
	struct stack_st *mem;

	mem = malloc(sizeof(intptr_t)*volume);
	mem->memsize = sizeof(intptr_t)*volume;
	mem->sp = 0;
	mem->volume = volume;
	pthread_mutex_init(&mem->mutex, NULL);
	pthread_cond_init(&mem->cond, NULL);
	return mem;
}

int stack_delete(ds_stack_t *p)
{
	struct stack_st *stack = p;

	pthread_mutex_destroy(&stack->mutex);
	pthread_cond_destroy(&stack->cond);
	free(stack);
	return 0;
}

int stack_push(ds_stack_t *p, intptr_t data)
{
	struct stack_st *stack = p;

	pthread_mutex_lock(&stack->mutex);
	while (stack->sp>=stack->volume) {
		pthread_cond_wait(&stack->cond, &stack->mutex);
	}
	stack->data[stack->sp] = data;
	stack->sp++;
	pthread_cond_signal(&stack->cond);
	pthread_mutex_unlock(&stack->mutex);
	return 0;
}

int stack_push_nb(ds_stack_t *p, intptr_t data)
{
	struct stack_st *stack = p;

	pthread_mutex_lock(&stack->mutex);
	if (stack->sp>=stack->volume) {
		pthread_mutex_unlock(&stack->mutex);
		return -EAGAIN;
	}
	stack->data[stack->sp] = data;
	stack->sp++;
	pthread_cond_signal(&stack->cond);
	pthread_mutex_unlock(&stack->mutex);
	return 0;
}


intptr_t stack_pop(ds_stack_t *p)
{
	struct stack_st *stack = p;
	intptr_t ret;

	pthread_mutex_lock(&stack->mutex);
	while (stack->sp==0) {
		pthread_cond_wait(&stack->cond, &stack->mutex);
	}
	stack->sp--;
	ret = stack->data[stack->sp];
	pthread_cond_signal(&stack->cond);
	pthread_mutex_unlock(&stack->mutex);
	return ret;
}

intptr_t stack_pop_nb(ds_stack_t *p)
{
	struct stack_st *stack = p;
	intptr_t ret;

	pthread_mutex_lock(&stack->mutex);
	if (stack->sp==0) {
		pthread_mutex_unlock(&stack->mutex);
		return 0;
	}
	stack->sp--;
	ret = stack->data[stack->sp];
	pthread_cond_signal(&stack->cond);
	pthread_mutex_unlock(&stack->mutex);
	return ret;
}

