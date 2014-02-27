#ifndef HTTP_H
#define HTTP_H

#include "ds_memvec.h"
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

#endif

