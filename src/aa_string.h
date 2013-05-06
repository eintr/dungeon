#ifndef AA_STRING_H
#define AA_STRING_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "aa_memvec.h"

typedef memvec_t string_t;

string_t * string_fromc(const char*);

char *string_toc(string_t*);

//int string_frommemvec(string_t *, const memvec_t*);

int string_cmp(string_t*, string_t*);

int string_cpy(string_t*, string_t*);

#define	string_serialize(x)	string_toc(x)

#endif

