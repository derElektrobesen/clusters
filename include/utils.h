#ifndef UTILS_H
#define UTILS_H


#if __STDC_VERSION__ >= 199901L
#define _XOPEN_SOURCE 600
#else
#define _XOPEN_SOURCE 500
#endif /* __STDC_VERSION__ */

#ifndef __USE_POSIX
#define __USE_POSIX
#endif

#include <sys/types.h>

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void clock_gettime_impl(struct timespec *ts);

#define _warn(fmt, ...) ({									\
	struct timespec time;									\
	clock_gettime_impl(&time);								\
												\
	char time_str[32];									\
	struct tm tm;										\
	localtime_r(&time.tv_sec, &tm);								\
												\
	size_t ret = strftime(time_str, sizeof(time_str), "%d.%m.%Y %H:%M:%S", &tm);		\
	snprintf(time_str + ret, sizeof(time_str) - ret, ".%.06ld", time.tv_nsec / 1000);	\
												\
	printf("[%s] " fmt " [%s:%d]\n", time_str, ## __VA_ARGS__, __FILE__, __LINE__);		\
})

#define log_e(fmt, ...) _warn("[E] " fmt, ## __VA_ARGS__)
#define log_w(fmt, ...) _warn("[W] " fmt, ## __VA_ARGS__)
#define log_i(fmt, ...) _warn("[I] " fmt, ## __VA_ARGS__)
#define log_d(fmt, ...) _warn("[D] " fmt, ## __VA_ARGS__)
#define log_t(fmt, ...) _warn("[T] " fmt, ## __VA_ARGS__)

#endif
