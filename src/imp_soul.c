#include <stdio.h>
#include <stdlib.h>

#include "imp.h"

imp_soul_t *imp_soul_new(void)
{
	return malloc(sizeof(imp_soul_t));
}

int imp_soul_delete(imp_soul_t *s)
{
	free(s);
	return 0;
}

