#include <unistd.h>
#include <sys/syscall.h>
#include "util_syscall.h"

pid_t gettid(void)
{
	    return syscall(SYS_gettid);
}

