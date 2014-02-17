#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aa_http.h"
#include "util_log.h"

typedef enum {
	HEADER_METHOD = 0,
	HEADER_URL,
	HEADER_VERSION,
	HEADER_HOST,
	HEADER_PARSE_DONE
} header_state;

int http_header_parse(struct http_header_st *hh, char * data) 
{
	char *start, *tmp;
	int size, hsize;

	hsize = strlen(data);
	
	if (hsize == 0) {
		mylog(L_ERR, "header parse: blank header data.");
		return -1;
	}

	hh->method.ptr = NULL;
	hh->version.ptr = NULL;
	hh->host.ptr = NULL;
	hh->original_hdr.ptr = (uint8_t *)data;
	hh->original_hdr.size = hsize;

	header_state state = HEADER_METHOD;
	while (state != HEADER_PARSE_DONE) {
		switch (state) {
			case HEADER_METHOD:
				start = data;
				tmp = strchr(data, ' ');
				if (!tmp) {
					mylog(L_ERR, "parse header get method failed");
					goto failed;
				}
				size = tmp - start;
				hh->method.ptr = (uint8_t *)start;
				hh->method.size = size;

				start = tmp + 1;
				state = HEADER_URL;
				break;
			case HEADER_URL:
				tmp = strchr(start, ' ');
				if (!tmp) {
					mylog(L_ERR, "parse header get url failed");
					goto failed;
				}
				size = tmp - start;
				url_brokedown(&hh->url, start, size);
				start = tmp + 1;
				state = HEADER_VERSION;
				break;
			case HEADER_VERSION:
				tmp = strstr(start, "\r\n");
				if (!tmp) {
					mylog(L_ERR, "parse header get version failed");
					goto failed;
				}
				size = tmp - start;
				hh->version.ptr = (uint8_t *)start;
				hh->version.size = size;
				start = tmp + 2;
				state = HEADER_HOST;
				break;
			case HEADER_HOST:
				tmp = strstr(start, "Host");
				if (tmp == NULL) {
					//log error
					mylog(L_ERR, "parse header get host failed");
					goto failed;
				}
				start = tmp + 6; //skip "Host: "
				tmp = strstr(start, "\r\n");
				size = tmp - start;
				hh->host.ptr = (uint8_t *)start;
				hh->host.size = size;
				state = HEADER_PARSE_DONE;
				break;
			case HEADER_PARSE_DONE:
				break;
			default:
				//mylog(L_WARNING, "header parse: Unknown state, report this bug please.");
				break;
		}
	}

	return 0;

failed:
	return -1;
}


#ifdef AA_HTTP_TEST

int main() 
{
	char *data= "POST /sdf?sdf HTTP/1.1\r\nUser-Agent: curl/7.15.5 (x86_64-redhat-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8b zlib/1.2.3 libidn/0.6.5\r\nHost: localhost:5000\r\nAccept: */*\r\nContent-Length: 3\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\nhah\r\n";
	struct http_header_st hh;
	int ret;
	ret = http_header_parse(&hh, data);
	printf("result is %d\n", ret);

	return 0;
}

#endif

