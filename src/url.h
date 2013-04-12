#ifndef URL_BROKEDOWN_H
#define URL_BROKEDOWN_H

#include "aa_string.h"

/*
 * A broken down url.
 */
struct url_brokedown_st {
	string_t *protocol;		// Such as: "http", "https", "ftp", ...
	string_t *host;			// Such as: "www.abc.com"
	uint16_t port;			// Port number, in host byteorder
	string_t *api;			// Such as: "/aaa/bbb/ccc/asd.php"
	string_t *param;		// Such as: "?param1=123&param2=casda"
};

/*
 * Create an url struct
 */
struct url_brokedown_st *url_new(void);

/*
 * Break down a header string into an url struct
 */
int url_brokedown(struct url_brokedown_st *, const char *url);

/*
 * Modify a broke down header, remove some parts which needed only by the proxy
 */
int url_deproxy(struct url_brokedown_st*);

/*
 * Destroy an url struct
 */
int url_delete(struct url_st *);

/*
 * Translate an url struct into a c string
 */
char *url_cstr(struct url_brokedown_st*);

#endif

