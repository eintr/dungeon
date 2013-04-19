#ifndef AA_STRING_H
#define AA_STRING_H

#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "aa_memvec.h"

typedef memvec_t string_t;

string_t *aa_string_fromc(const char*);

int string_cmp(string_t*, string_t*);

int string_cpy(string_t*, string_t*);

char *string_toc(string_t*);

#define	string_serialize(x)	string_toc(x)

#endif

