#include "aa_syscall.h"

pid_t gettid(void)
{
	    return syscall(SYS_gettid);
}

