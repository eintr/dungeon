#include "http.h"

struct http_header_st *  http_header_parse(char * data) 
{
	memvec_t *method, *version, *host;
	struct http_header_st *hh;
	char *start ,*end, *tmp;
	int size, hsize;
	
	hsize = strlen(data);

	hh = (struct http_header_st *) malloc(sizeof(struct http_header_st));
	if (hh == NULL) {
		//log error;
		return NULL;
	}
	hh->method = NULL;
	hh->version = NULL;
	hh->host = NULL;
	hh->original_hdr = memvec_new(data, hsize);
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
				method = memvec_new(start, size);
				if (method == NULL) {
					//log error
					goto failed;
				}
				hh->method = method;
				start = tmp + 1;
				state = HEADER_URL;
				break;
			case HEADER_URL:
				tmp = strchr(start, ' ');
				size = tmp - start;
				//TODO: init url;
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
				start = tmp + 2
				state = HEADER_HOST;
				break;
			case HEADER_HOST:
				tmp = strstr(start, "Host");
				if (tmp == NULL) {
					//log error
					goto failed;
				}
				start = tmp;
				tmp = strstr(start, "\r\n");
				size = tmp - start;
				host = memvec_new(start, size);
				if (host == NULL) {
					//log error
					goto failed;
				}
				hh->host = host;
				state = HEADER_PARSE_DONE;
				break;
		}
	}
	
	return hh;

failed:
	if (hh->method) 
		memvec_delete(hh->method);
	if (hh->version)
		memvec_delete(hh->version);
	if (hh->host)
		memvec_delete(hh->host);
	if (original_hdr) 
		memvec_delete(original_hdr);
	free(hh);

 	return NULL;
}


#ifdef AA_HTTP_TEST

int main() 
{
	char *data= "POST /sdf?sdf HTTP/1.1\r\nUser-Agent: curl/7.15.5 (x86_64-redhat-linux-gnu) libcurl/7.15.5 OpenSSL/0.9.8b zlib/1.2.3 libidn/0.6.5\r\nHost: localhost:5000\r\nAccept: */*\r\nContent-Length: 3\r\nContent-Type: application/x-www-form-urlencoded\r\n\r\nhah\r\n";
	return 0;
}

#endif
