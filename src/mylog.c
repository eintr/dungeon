#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define SYSLOG_NAMES 1
#include "mylog.h"

static FILE *flog_fp;
static FILE *clog_fp;
//static int slog_facility;
//static int slog_ident;

static int log_target_set = 0;
static int log_targets_count = 0;

int mylog_set_target(enum log_target_en code, ...)
{
	va_list va;
	FILE *fp;
	char *ident;
	int fac;

	if (code & LOGTARGET_FILE) {
		va_start(va, code);
		fp = fopen(va_arg(va, char*), "a");
		if (fp==NULL) {
			va_end(va);
			return -1;
		}
		setvbuf(fp, NULL, _IOLBF, 0);
		if (flog_fp!=NULL) {
			fclose(flog_fp);
			flog_fp = fp;
			return log_targets_count;
		} else {
			flog_fp = fp;
			va_end(va);
			log_target_set |= LOGTARGET_FILE;
			return ++log_targets_count;
		}
	}
	if (code & LOGTARGET_SYSLOG) {
		va_start(va, code);
		ident = va_arg(va, char*);
		fac = va_arg(va, int);
		openlog(ident, LOG_PID, fac);
		va_end(va);
		log_target_set |= LOGTARGET_SYSLOG;
		return ++log_targets_count;
	}
	if (code & LOGTARGET_CONSOLE) {
		if (clog_fp==NULL) {
			clog_fp = fopen("/dev/console", "a");
			if (clog_fp==NULL) {
				return -1;
			}
			log_target_set |= LOGTARGET_CONSOLE;
			return ++log_targets_count;
		}
		return log_targets_count;
	}
	if (code & LOGTARGET_STDERR) {
		if (!(log_target_set & LOGTARGET_STDERR)) {
			log_target_set |= LOGTARGET_STDERR;
			return ++log_targets_count;
		}
		return log_targets_count;
	}
	return -1;
}

int mylog_clear_target(enum log_target_en code)
{
	if (code & LOGTARGET_FILE) {
		if (log_target_set & LOGTARGET_FILE) {
			log_target_set &= ~LOGTARGET_FILE;
			fclose(flog_fp);
			return --log_targets_count;
		}
		return log_targets_count;
	}
	if (code & LOGTARGET_SYSLOG) {
		if (log_target_set & LOGTARGET_SYSLOG) {
			log_target_set &= ~LOGTARGET_SYSLOG;
			return --log_targets_count;
		}
		return log_targets_count;
	}
	if (code & LOGTARGET_CONSOLE) {
		if (log_target_set & LOGTARGET_CONSOLE) {
			fclose(clog_fp);
			log_target_set &= ~LOGTARGET_CONSOLE;
			return --log_targets_count;
		}
		return log_targets_count;
	}
	if (code & LOGTARGET_STDERR) {
		if (log_target_set & LOGTARGET_STDERR) {
			log_target_set &= ~LOGTARGET_STDERR;
			return --log_targets_count;
		}
		return log_targets_count;
	}
	return -1;
}

void do_mylog(int loglevel, const char *fmt, ...)
{
	va_list va, va1;

	va_start(va, fmt);
	if (log_target_set & LOGTARGET_FILE) {
		fprintf(flog_fp, "%s[%d]: ", APPNAME, getpid());
		va_copy(va1, va);
		vfprintf(flog_fp, fmt, va1);
		va_end(va1);
		if (fmt[strlen(fmt)-1]!='\n') {
			fputc('\n', flog_fp);
		}
	}
	if (log_target_set & LOGTARGET_STDERR) {
		fprintf(stderr, "%s[%d]: ", APPNAME, getpid());
		va_copy(va1, va);
		vfprintf(stderr, fmt, va1);
		va_end(va1);
		if (fmt[strlen(fmt)-1]!='\n') {
			fputc('\n', stderr);
		}
	}
	if (log_target_set & LOGTARGET_SYSLOG) {
		va_copy(va1, va);
		vsyslog(loglevel, fmt, va1);
		va_end(va1);
	}
	if (log_target_set & LOGTARGET_CONSOLE) {
		fprintf(clog_fp, "%s[%d]: ", APPNAME, getpid());
		va_copy(va1, va);
		vfprintf(clog_fp, fmt, va1);
		va_end(va1);
		if (fmt[strlen(fmt)-1]!='\n') {
			fputc('\n', clog_fp);
		}
	}
	va_end(va);
}

void mylog_reset(void)
{
	mylog_clear_target(LOGTARGET_FILE);
	mylog_clear_target(LOGTARGET_SYSLOG);
	mylog_clear_target(LOGTARGET_CONSOLE);
	mylog_clear_target(LOGTARGET_STDERR);
}

int get_log_value(const char *str)
{
	int i;
	for (i=0;facilitynames[i].c_val!=-1;++i) {
		if (strcmp(facilitynames[i].c_name, str)==0) {
			return facilitynames[i].c_val;
		}
	}
	return -1;
}
