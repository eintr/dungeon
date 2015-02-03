/** \cond 0 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
/** \endcond */

/** \file mod.c
	This is a test module, it reflects every byte of the socket.
*/

#include <room_service.h>

static int debug_accept=0;
static int debug_rcv=0;
static int debug_snd=0;

struct listener_memory_st {
	int listen_sd;
};

#define	BUFSIZE	32768

struct fakehttpd_memory_st {
	int sd;
	int state;

	int doc_fd;

	uint8_t header[BUFSIZE];
	size_t header_len, header_pos;

	char body_buf[BUFSIZE];
	size_t body_len, body_pos;

	uint8_t memoirs[BUFSIZE];
};

static imp_soul_t listener_soul, echoer_soul;
static struct addrinfo *local_addr;
static struct timeout_msec_st {
    uint32_t recv, send;
    uint32_t connect;
    uint32_t accept;
} timeo;
static char *docpath;

static imp_t *id_listener;

static int get_config(cJSON *conf)
{
	int ga_err;
	struct addrinfo hint;
	cJSON *value;
	char *Host, Port[12];
	

	hint.ai_flags = 0;
	hint.ai_family = AF_INET;
	hint.ai_socktype = SOCK_STREAM;
	hint.ai_protocol = IPPROTO_TCP;

	value = cJSON_GetObjectItem(conf, "Enabled");
	if (value->type != cJSON_True) {
		mylog(L_INFO, "Module is configured disabled.");
		return -1;
	}

	value = cJSON_GetObjectItem(conf, "DocPath");
	if (value->type != cJSON_String) {
		mylog(L_ERR, "Module is configured with illegal DocPath.");
		return -1;
	} else {
		docpath = value->valuestring;
	}

	value = cJSON_GetObjectItem(conf, "LocalAddr");
	if (value->type != cJSON_String) {
		mylog(L_ERR, "Module is configured with illegal LocalAddr.");
		return -1;
	} else {
		Host = value->valuestring;
	}

	value = cJSON_GetObjectItem(conf, "LocalPort");
	if (value->type != cJSON_Number) {
		mylog(L_ERR, "Module is configured with illegal LocalPort.");
		return -1;
	} else {
		sprintf(Port, "%d", value->valueint);
	}

	ga_err = getaddrinfo(Host, Port, &hint, &local_addr);
	if (ga_err) {
		mylog(L_ERR, "Module configured with illegal LocalAddr and LocalPort.");
		return -1;
	}

	value = cJSON_GetObjectItem(conf, "TimeOut_Recv_ms");
	if (value->type != cJSON_Number) {
		mylog(L_ERR, "Module is configured with illegal TimeOut_Recv_ms.");
		return -1;
	} else {
		timeo.recv = value->valueint;
		mylog(L_INFO, "Module is configured with TimeOut_Recv_ms=%d.", timeo.recv);
	}

	value = cJSON_GetObjectItem(conf, "TimeOut_Send_ms");
	if (value->type != cJSON_Number) {
		mylog(L_ERR, "Module is configured with illegal TimeOut_Send_ms.");
		return -1;
	} else {
		timeo.send = value->valueint;
		mylog(L_INFO, "Module is configured with TimeOut_Send_ms=%d.", timeo.send);
	}

	return 0;
}

static int delta_t(void)
{
	return time(NULL)-dungeon_heart->create_time.tv_sec;
}

static ds_stack_t *mem_cache=NULL;

static struct fakehttpd_memory_st *mem_alloc(void)
{
	struct fakehttpd_memory_st *mem;

	mem = stack_pop(mem_cache);
	if (mem==NULL) {
		mem=calloc(1, sizeof(*mem));
	}
	return mem;
}

static void mem_free(struct fakehttpd_memory_st *p)
{
	if (stack_push(mem_cache, p)!=0) {
		free(p);
	}
}

static int mod_init(cJSON *conf)
{
	struct listener_memory_st *l_mem;

	if (get_config(conf)!=0) {
		return -1;
	}

	mem_cache = stack_new(20000);

	l_mem = calloc(sizeof(*l_mem), 1);
	if (l_mem==NULL) {
		return -1;
	}
	id_listener = imp_summon(l_mem, &listener_soul);
	if (id_listener==NULL) {
		mylog(L_ERR, "imp_summon() Failed!\n");
		return 0;
	}
	return 0;
}

static int mod_destroy(void)
{
	stack_delete(mem_cache);
	imp_kill(id_listener);
	return 0;
}

static void mod_maint(void)
{
	//fprintf(stderr, "%s is running.\n", __FUNCTION__);
}

static cJSON *mod_serialize(void)
{
	//fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

/**************************************************************/

static void *listener_new(struct listener_memory_st *lmem)
{
	int val=1;
	int sdflags;

	lmem->listen_sd = socket(AF_INET, SOCK_STREAM, 0);
	if (lmem->listen_sd<0) {
		mylog(L_ERR, "socket() Failed: %m\n");
		free(lmem);
		return NULL;
	}

	setsockopt(lmem->listen_sd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(val));

    sdflags = fcntl(lmem->listen_sd, F_GETFL);
    fcntl(lmem->listen_sd, F_SETFL, sdflags|O_NONBLOCK);

    if (bind(lmem->listen_sd, local_addr->ai_addr, local_addr->ai_addrlen)<0) {
        close(lmem->listen_sd);
        free(lmem);
        return NULL;
    }

	if (listen(lmem->listen_sd, 128)<0) {
		mylog(L_ERR, "listen() Failed: %m\n");
		close(lmem->listen_sd);
		free(lmem);
		return -1;
	}
	return NULL;
}

static int listener_delete(struct listener_memory_st *mem)
{
	mylog(L_DEBUG, "%s is running, free memory.\n", __FUNCTION__);
	free(mem);
	return 0;
}

static enum enum_driver_retcode listener_driver(struct listener_memory_st *lmem)
{
	static int count =10;
	struct fakehttpd_memory_st *emem;
	int newsd;
	imp_t *echoer;

	while ((newsd = accept(lmem->listen_sd, NULL, NULL))>=0) {
		emem = mem_alloc();
		emem->sd = newsd;
		echoer = imp_summon(emem, &echoer_soul);
		if (echoer==NULL) {
			mylog(L_ERR, "Failed to summon a new imp.");
			close(newsd);
			mem_free(emem);
			continue;
		}
	}
	if (errno==EAGAIN) {
		imp_set_ioev(lmem->listen_sd, EPOLLIN|EPOLLOUT|EPOLLRDHUP);
		imp_set_timer(5111);
		return TO_BLOCK;
	} else {
		return TO_TERM;
	}
}

static void *listener_serialize(struct listener_memory_st *m)
{
	return NULL;
}


enum state_en {
	ST_RECV=1,
	ST_PREPARE_HEADER,
	ST_SEND_HEADER,
	ST_BODY_READ,
	ST_BODY_SEND,
	ST_TERM,
	ST_Ex
};

static char *fake_http_header_fmt =
	"HTTP/1.1 200 OK\r\n"
	"Content-Type: text/plain; charset=utf-8\r\n"
	"Content-Length: %zu\r\n\r\n";

static void *echo_new(struct fakehttpd_memory_st *m)
{
	struct sockaddr_storage peer_addr;
	socklen_t peer_addr_len;
	char ipstr[40];
	int port;

	peer_addr_len = sizeof(peer_addr);
	getpeername(m->sd, (void*)&peer_addr, &peer_addr_len);
	port = ntohs(((struct sockaddr_in*)(&peer_addr))->sin_port);
	inet_ntop(AF_INET, &((struct sockaddr_in*)(&peer_addr))->sin_addr, ipstr, 40);
	snprintf(m->memoirs, BUFSIZE, "imp[%d] for %s:%d (+%ds)->", IMP_ID, ipstr, port, delta_t());

	m->state = ST_RECV;
	m->doc_fd = open(docpath, O_RDONLY);
	if (m->doc_fd<0) {
		mylog(L_ERR, "open(docpath): %m");
		m->state = ST_Ex;
	}
	return NULL;
}

static int echo_delete(struct fakehttpd_memory_st *memory)
{
	char log[1024];

	close(memory->sd);
	close(memory->doc_fd);
	sprintf(log, "[+%ds] echo_delete() close(sd) .", delta_t());
	strcat(memory->memoirs, log);
	//fprintf(stderr, "%s\n", memory->memoirs);
	free(memory);
	return 0;
}

#define	min(X, Y)	((X)>(Y)?(Y):(X))

static enum enum_driver_retcode echo_driver(struct fakehttpd_memory_st *mem)
{
	uint8_t buf[BUFSIZE], log[BUFSIZE];
	off_t len, pos;
	ssize_t ret;
	struct stat docstat;

	switch (mem->state) {
		case ST_RECV:
			if (IMP_TIMEDOUT) {
				sprintf(log, "ST_RECV[+%ds] timed out->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			len = recv(mem->sd, buf, BUFSIZE, MSG_DONTWAIT);
			if (len == 0) {
				sprintf(log, "ST_RECV[+%ds] EOF->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_TERM;
				return TO_RUN;
			} else if (len < 0) {
				if (errno==EAGAIN) {
					sprintf(log, "ST_RECV[+%ds] sleep->", delta_t());
					strcat(mem->memoirs, log);
					imp_set_ioev(mem->sd, EPOLLIN|EPOLLRDHUP);
					imp_set_timer(timeo.recv);
					return TO_BLOCK;
				} else {
					sprintf(log, "ST_RECV[+%ds] error: %m->", delta_t());
					strcat(mem->memoirs, log);
					mem->state = ST_Ex;
					return TO_RUN;
				}
			} else {
				sprintf(log, "ST_RECV[+%ds] OK->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_PREPARE_HEADER;
				debug_rcv++;
				return TO_RUN;
			}
			break;
		case ST_PREPARE_HEADER:

			fstat(mem->doc_fd, &docstat);

			mem->header_len = snprintf(mem->header, BUFSIZE, fake_http_header_fmt, docstat.st_size);
			if (mem->header_len==BUFSIZE) {
				sprintf(log, "ST_PREPARE_HEADER[+%ds] Header is too long!->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			sprintf(log, "ST_PREPARE_HEADER[+%ds] OK->", delta_t());
			strcat(mem->memoirs, log);
			mem->header_pos = 0;
			mem->state = ST_SEND_HEADER;
			return TO_RUN;
			break;
		case ST_SEND_HEADER:
			if (IMP_TIMEDOUT) {
				sprintf(log, "ST_SEND_HEADER[+%ds] timed out or error->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			ret = send(mem->sd, mem->header + mem->header_pos, mem->header_len, MSG_DONTWAIT);
			if (ret<=0) {
				if (errno==EAGAIN) {
					sprintf(log, "ST_SEND_HEADER[+%ds] sleep->", delta_t());
					strcat(mem->memoirs, log);
					imp_set_ioev(mem->sd, EPOLLOUT|EPOLLRDHUP);
					imp_set_timer(timeo.recv);
					return TO_BLOCK;
				} else {
					sprintf(log, "ST_SEND_HEADER[+%ds] error: %m->", delta_t());
					strcat(mem->memoirs, log);
					mem->state = ST_Ex;
					return TO_RUN;
				}
			} else {
				mem->header_pos += ret;
				mem->header_len -= ret;
				if (mem->header_len == 0) {
					sprintf(log, "ST_SEND_HEADER[+%ds] %d bytes sent, OK->", delta_t(), ret);
					strcat(mem->memoirs, log);
					mem->state = ST_BODY_READ;
					return TO_RUN;
				}
				sprintf(log, "ST_SEND_HEADER[+%ds] %d bytes sent, again->", delta_t(), ret);
				strcat(mem->memoirs, log);
				return TO_RUN;
			}
			break;
		case ST_BODY_READ:
			mem->body_len = read(mem->doc_fd, mem->body_buf, BUFSIZE);
			if (mem->body_len<0) {
				sprintf(log, "ST_BODY_READ[+%ds] error(%m)->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_Ex;
				return TO_RUN;
			} else if (mem->body_len==0) {
				sprintf(log, "ST_BODY_READ[+%ds] doc_fd EOF->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_TERM;
				return TO_RUN;
			} else {
				mem->body_pos = 0;
				mem->state = ST_BODY_SEND;
				return TO_RUN;
			}
			break;
		case ST_BODY_SEND:
			if (IMP_TIMEDOUT) {
				sprintf(log, "ST_SEND_BODY[+%ds] timed out or error->", delta_t());
				strcat(mem->memoirs, log);
				mem->state = ST_Ex;
				return TO_RUN;
			}
			ret = send(mem->sd, mem->body_buf + mem->body_pos, mem->body_len, MSG_DONTWAIT);
			if (ret<=0) {
				if (errno==EAGAIN) {
					sprintf(log, "ST_SEND_BODY[+%ds] sleep->", delta_t());
					strcat(mem->memoirs, log);
					imp_set_ioev(mem->sd, EPOLLOUT|EPOLLRDHUP);
					imp_set_timer(timeo.send);
					return TO_BLOCK;
				} else {
					sprintf(log, "ST_SEND_BODY[+%ds] error: %m->", delta_t());
					strcat(mem->memoirs, log);
					mem->state = ST_Ex;
					return TO_RUN;
				}
			} else {
				mem->body_len -= ret;
				mem->body_pos += ret;
				if (mem->body_len <= 0) {
					sprintf(log, "ST_SEND_BODY[+%ds] %d bytes sent, over ->", delta_t(), ret);
					strcat(mem->memoirs, log);
					mem->state = ST_BODY_READ;
					debug_snd++;
					return TO_RUN;
				} else {
					sprintf(log, "ST_SEND_BODY[+%ds] %d bytes sent, again->", delta_t(), ret);
					strcat(mem->memoirs, log);
					return TO_RUN;
				}
			}
			break;
		case ST_Ex:
			sprintf(log, "ST_Ex[+%ds] Exception happened!", delta_t());
			strcat(mem->memoirs, log);
			mem->state = ST_TERM;
			break;
		case ST_TERM:
			return TO_TERM;
			break;
		default:
			break;
	}
	return TO_RUN;
}

static void *echo_serialize(struct fakehttpd_memory_st *mem)
{
	fprintf(stderr, "%s is running.\n", __FUNCTION__);
	return NULL;
}

static imp_soul_t listener_soul = {
	.fsm_new = listener_new,
	.fsm_delete = listener_delete,
	.fsm_driver = listener_driver,
	.fsm_serialize = listener_serialize,
};

static imp_soul_t echoer_soul = {
	.fsm_new = echo_new,
	.fsm_delete = echo_delete,
	.fsm_driver = echo_driver,
	.fsm_serialize = echo_serialize,
};

module_interface_t MODULE_INTERFACE_SYMB = 
{
	.mod_initializer = mod_init,
	.mod_destroier = mod_destroy,
	.mod_maintainer = mod_maint,
	.mod_serialize = mod_serialize,
};

