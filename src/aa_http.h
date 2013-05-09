#ifndef HTTP_H
#define HTTP_H

#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "aa_memvec.h"
#include "aa_url.h"

#define HTTP_HEADER_MAX 4096

struct http_header_st {
	memvec_t method;
	struct url_brokedown_st url;
	memvec_t version;
	memvec_t host;

	memvec_t original_hdr;
};

int http_header_parse(struct http_header_st *, char *);
typedef enum {
	HEADER_METHOD = 0,
	HEADER_URL,
	HEADER_VERSION,
	HEADER_HOST,
	HEADER_PARSE_DONE
} header_state;

#endif

