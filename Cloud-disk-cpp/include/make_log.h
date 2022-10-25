#ifndef  _MAKE_LOG_H_
#define  _MAKE_LOG_H
#include "pthread.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C"
{
#endif

int out_put_file(char *path, char *buf);
int make_path(char *path, const char *module_name, const char *proc_name);
int dumpmsg_to_file(const char *module_name, const char *proc_name, const char *filename, const int line, const char *funcname, const char *fmt, ...);
#ifndef _LOG
#define LOG(module_name, proc_name, fmt, ...) \
        do{ \
		dumpmsg_to_file(module_name, proc_name, __FILE__, __LINE__, __FUNCTION__, fmt, ##__VA_ARGS__);\
	}while(0)
#else
#define LOG(module_name, proc_name, x...)
#endif

extern pthread_mutex_t ca_log_lock;

#endif

#ifdef __cplusplus
}
#endif




