#ifndef HTTP_H
#define HTTP_H

struct http_header_st {
	memvec_t *method;
	url_st url;
	memvec_t *version;
	memvec_t *host;

	memvec_t *original_hdr;
};

typedef enum {
	HEADER_METHOD = 0,
	HEADER_URL,
	HEADER_VERSION,
	HEADER_HOST,
	HEADER_PARSE_DONE
} header_state;

struct http_header_st *http_header_parse(char *);

#endif

