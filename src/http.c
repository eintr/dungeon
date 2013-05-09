#include "http.h"


int http_header_parse(struct http_header_st *hh, char * data) 
{
	char *start ,*end, *tmp;
	int size, hsize;

	hsize = strlen(data);

	hh->method.ptr = NULL;
	hh->version.ptr = NULL;
	hh->host.ptr = NULL;
	hh->original_hdr.ptr = data;
	hh->original_hdr.size = hsize;

	if (hh->original_hdr == NULL) {
		//log error;
		goto failed;
	}

	header_state state = HEADER_METHOD;
	while (state != HEADER_PARSE_DONE) {
		switch (state) {
			case HEADER_METHOD:
				start = data;
				tmp = strchr(data, ' ');
				size = tmp - start;
				hh->method.ptr = start;
				hh->method.size = size;

				start = tmp + 1;
				state = HEADER_URL;
				break;
			case HEADER_URL:
				tmp = strchr(start, ' ');
				size = tmp - start;
				url_init(&hh->url, start, size);
				start = tmp + 1;
				state = HEADER_VERSION;
				break;
			case HEADER_VERSION:
				tmp = strstr(start, "\r\n");
				size = tmp - start;
				version = memvec_new(start, size);
				if (version == NULL) {
					//log error
					goto failed;
				}
				hh->version = version;
				start = tmp + 2;
				state = HEADER_HOST;
				break;
			case HEADER_HOST:
				tmp = strstr(start, "Host");
				if (tmp == NULL) {
					//log error
					goto failed;
				}
				start = tmp + 6; //skip "Host: "
				tmp = strstr(start, "\r\n");
				size = tmp - start;
				hh->host.ptr = start;
				hh->host.size = size;
				state = HEADER_PARSE_DONE;
				break;
			case HEADER_PARSE_DONE:
				break;
			default:
				// log
				abort();
				break;
		}
	}

	return 0;

failed:
	if (hh->method) 
		memvec_delete(hh->method);
	if (hh->version)
		memvec_delete(hh->version);
	if (hh->host)
		memvec_delete(hh->host);
	if (hh->original_hdr) 
		memvec_delete(hh->original_hdr);

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

