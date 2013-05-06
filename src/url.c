#include <stdio.h>
#include <stdlib.h>

#include "url.h"

int url_brokedown(struct url_brokedown_st *result, const char *str)
{
	uint8_t *pos, *ptr, *port_asciiz, *url=(void*)str;
	memvec_t tmp, host, port, path, param;

	pos = strchr(url, ':');
	if (pos!=NULL) {
		result->protocol.ptr = url;
		result->protocol.size = pos - url;

		host.ptr = pos+3;
		pos = strchr(host.ptr, '/');
		host.size = pos - host.ptr;
		for (ptr=host.ptr;ptr<pos;ptr++) {
			if (*ptr==':') {
				break;
			}
		}
		if (ptr==pos) {	// : not found, port is 80
			result->host.ptr = host.ptr;
			result->host.size = host.size;
			result->port = 0;  // not 80 ? 
		} else { // : found, extract host:port
			host.size = ptr-host.ptr;
			result->host.ptr = host.ptr;
			result->host.size = host.size;
			ptr++;
			port.ptr = ptr;
			port.size = pos-ptr;

			port_asciiz = string_toc(&port);
			result->port = atoi(port_asciiz);
			free(port_asciiz);
		}
		pos++; // move pos to the beginning of path
	} else { // There is NO 'http://' part
		result->protocol.ptr = NULL;
		result->protocol.size = 0;
		result->host.ptr = NULL;
		result->host.size = 0;
		result->port = 0;
		pos = url; //move pos to url ?? 127.0.0.1:8080/xxx ?
	}
	// pos points to the beginning of path here.
	tmp.ptr = pos;
	pos = strchr(tmp.ptr, '?');
	if (pos==NULL) {	// No parameters found
		result->path.ptr = tmp.ptr;
		result->path.size = strlen(tmp.ptr);
		result->param.ptr = NULL;
		result->param.size = 0;
	} else {
		result->path.ptr = tmp.ptr;
		result->path.size = pos - tmp.ptr;
		result->param.ptr = pos;
		result->param.size = strlen(pos);
	}

	return 0;
}


