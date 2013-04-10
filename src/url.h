#ifndef URL_BROKEDOWN_H
#define URL_BROKEDOWN_H

#include "aa_string.h"

struct url_brokedown_st {
	string_t *method;
	string_t *host;
	uint16_t port;			// In host byteorder
	string_t *api;
	string_t *param;
};

struct url_brokedown_st *url_new(void);

int url_brokedown(struct url_brokedown_st *, const char *url);

int url_deproxy(struct url_brokedown_st*);

int url_delete(struct url_st *);

char *url_cstr(struct url_brokedown_st*);

#endif

