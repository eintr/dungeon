#ifndef UTIL_EXCEPTION_H
#define UTIL_EXCEPTION_H

#include <setjmp.h>

extern jmp_buf root_cache;

void ex_init(void);

void ex_abort(void);

#endif

