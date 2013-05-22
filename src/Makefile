CFLAGS+=-g -O0 -pthread
LDFLAGS+=-pthread

TARGET=aaproxy

SRCS=aa_bufferlist.c aa_hasht.c aa_memvec.c aa_string.c main.c proxy_pool.c aa_conf.c aa_http.c aa_queue.c aa_url.c mylog.c server_state_dict.c aa_connection.c aa_llist.c aa_rbtree.c cJSON.c proxy_context.c

OBJS=$(SRCS:.c=.o)

all: ${TARGET}

${TARGET}: ${OBJS}

