#ifndef MYLOG_H
#define MYLOG_H

#include <syslog.h>
#include <string.h>
#include <assert.h>

enum log_target_en {
	LOGTARGET_FILE		=0x00000001L,
	LOGTARGET_SYSLOG	=0x00000002L,
	LOGTARGET_CONSOLE	=0x00000004L,
	LOGTARGET_STDERR	=0x00000008L,
};

enum log_prio {
	L_MINVALUE = LOG_ERR,

	L_ERR = LOG_ERR,
	L_NOTICE = LOG_NOTICE,
	L_WARNING = LOG_WARNING,
	L_INFO = LOG_INFO,
	L_DEBUG = LOG_DEBUG,

	L_MAXVALUE = LOG_DEBUG,
};

int mylog_set_target(enum log_target_en, ...); 
int mylog_clear_target(enum log_target_en);
int mylog_least_level(int loglevel);
void mylog_reset(void);

void do_mylog(int loglevel, const char *fmt, ...);

#define	LOG_BUFFER_SIZE	1024
extern __thread char log_buffer_[LOG_BUFFER_SIZE];

extern int log_base_level_;

/*
#define mylog(l, f, ...) do{							\
	char *_newf_;										\
	_newf_ = pthread_getspecific(key_tls_log_buffer_);	\
	if (_newf_==NULL) {									\
		_newf_ = malloc(LOG_BUFFER_SIZE);				\
		assert(_newf_);									\
		pthread_setspecific(key_tls_log_buffer_, _newf_);	\
	}														\
	snprintf(_newf_, LOG_BUFFER_SIZE, "%s/%s(%d): %s", __FILE__, __FUNCTION__, __LINE__, f);\
	do_mylog(l, _newf_, ##__VA_ARGS__);						\
}while(0)
*/
#ifndef RELEASE_BUILDING
#define mylog(level_, format_, ...) do{if(level_<=log_base_level_){snprintf(log_buffer_, LOG_BUFFER_SIZE, "%s/%s(%d): %s", __FILE__, __FUNCTION__, __LINE__, format_);do_mylog(level_, log_buffer_, ##__VA_ARGS__);}}while(0)
#else
#define mylog(level_, format_, ...) do{if(level_<=log_base_level_){do_mylog(level_, format_, ##__VA_ARGS__);}}while(0)
#endif

int get_log_value(const char*);

#endif

