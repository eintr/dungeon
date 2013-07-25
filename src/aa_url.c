#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aa_url.h"

int url_brokedown(url_st *result, const char *start, int size)
{
	void *str = (void *) start;
	uint8_t *pos, *ptr, *port_asciiz, *url;
	memvec_t tmp, host, port;

	url = malloc(size+1);
	if (url==NULL) {
		//log error
		return -1;
	}
	strncpy((void*)url, start, size);
	url[size] = '\0';

	result->protocol.ptr = NULL;
	result->protocol.size = 0;
	result->host.ptr = NULL;
	result->host.size = 0;
	result->port = 0;
	pos = url; 

	// pos points to the beginning of path here.
	tmp.ptr = pos;
	pos = (void*)strchr((void*)tmp.ptr, '?');
	if (pos==NULL) {	// No parameters found
		result->path.ptr = str + (tmp.ptr - url);
		result->path.size = strlen((void*)tmp.ptr);
		result->param.ptr = NULL;
		result->param.size = 0;
	} else {
		result->path.ptr = str + (tmp.ptr - url);
		result->path.size = pos - tmp.ptr;
		result->param.ptr = str + (pos - url) + 1;
		result->param.size = strlen((void*)pos) - 1;
	}

	free(url);
	return 0;
}

