#ifndef URL_BROKEDOWN_H
#define URL_BROKEDOWN_H

#include "ds_string.h"

/*
 * A broken down url.
 */
typedef struct url_brokedown_st {
	string_t protocol;		// Such as: "http", "https", "ftp", ...
	string_t host;			// Such as: "www.abc.com"
	uint16_t port;			// Port number, in host byteorder
	string_t path;			// Such as: "/aaa/bbb/ccc/asd.php"
	string_t param;			// Such as: "?param1=123&param2=casda"
} url_st;

/*
 * Break down a header string into an url struct
 */
int url_brokedown(url_st *result, const char *start, int size);

/*
 * Modify a broke down header, remove some parts which needed only by the proxy
 */
int url_deproxy(struct url_brokedown_st*);

/*
 * Translate an url struct into a c string
 */
char *url_cstr(struct url_brokedown_st*);

#endif

