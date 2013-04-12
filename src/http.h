#ifndef HTTP_H
#define HTTP_H

struct http_header_st {
	memvec_t *method;
	struct url_st url;
	memvec_t *version;
	memvec_t *host;

	memvec_t *original_hdr;
};

struct http_header_st *http_header_parse(char *);

#endif

