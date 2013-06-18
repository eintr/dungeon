#ifndef AA_STRING_H
#define AA_STRING_H

#include "aa_memvec.h"

#include "cJSON.h"

typedef memvec_t string_t;

string_t * string_fromc(const char*);

char *string_toc(string_t*);

//int string_frommemvec(string_t *, const memvec_t*);

int string_cmp(string_t*, string_t*);

int string_cpy(string_t*, string_t*);

cJSON *string_serialize(string_t*);

#endif

