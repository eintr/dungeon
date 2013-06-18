#ifndef MYLOG_H
#define MYLOG_H

#include <unistd.h>
#include <alloca.h>
#include <syslog.h>
#include <string.h>

enum log_target_en {
	LOGTARGET_FILE		=0x00000001L,
	LOGTARGET_SYSLOG	=0x00000002L,
	LOGTARGET_CONSOLE	=0x00000004L,
	LOGTARGET_STDERR	=0x00000008L,
};

enum log_prio {
	L_ERR = LOG_ERR,
	L_NOTICE = LOG_NOTICE,
	L_WARNING = LOG_WARNING,
	L_INFO = LOG_INFO,
	L_DEBUG = LOG_DEBUG,
};

int mylog_set_target(enum log_target_en, ...); 
int mylog_clear_target(enum log_target_en);
void mylog_reset(void);

void do_mylog(int loglevel, const char *fmt, ...);

//#define mylog(l, f, ...) do{char *_newf_;_newf_ = alloca(strlen(f)+256);snprintf(_newf_, strlen(f)+256, "%s/%s(%d): %s", __FILE__, __FUNCTION__, __LINE__, f);do_mylog(l, _newf_, ##__VA_ARGS__);}while(0)
#define mylog(l, f, ...) do{char *_newf_;_newf_ = malloc(strlen(f)+256);snprintf(_newf_, strlen(f)+256, "%s/%s(%d): %s", __FILE__, __FUNCTION__, __LINE__, f);do_mylog(l, _newf_, ##__VA_ARGS__);free(_newf_);}while(0)

int get_log_value(const char*);

#endif

