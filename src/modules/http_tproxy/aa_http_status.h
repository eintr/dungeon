#ifndef HTTP_STATUS_CODE_H
#define HTTP_STATUS_CODE_H

#include "aa_conf.h"

#define HTTP_503 "HTTP/1.1 503 Service Temporarily Unavailable\r\nServer: " APPNAME "\r\nContent-Length: 0\r\n\r\n"
#define HTTP_504 "HTTP/1.1 504 Gateway Timeout\r\nServer: " APPNAME "\r\nContent-Length: 0\r\n\r\n"


#endif
