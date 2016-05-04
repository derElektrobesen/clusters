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
#include <sys/time.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>

void clock_gettime_impl(struct timespec *ts);

#define KiB * 1024
#define Mb KiB * 1024
#define MAX_LOG_LINE (1 KiB)

#define STR(...) STR_I(__VA_ARGS__)
#define STR_I(...) #__VA_ARGS__

#define COMBINE2(A, B) A ## B
#define COMBINE(A, B) COMBINE2(A, B)

extern __thread char log_line[];

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
	pthread_t ptid = pthread_self();							\
	ret = snprintf(log_line, MAX_LOG_LINE,							\
		"[%s] [%10u] [%20s:%-4d] " fmt "\n", time_str, (unsigned)ptid,			\
			__FILE__, __LINE__, ## __VA_ARGS__);					\
	push_log(log_line, ret);								\
})

#define log_e(fmt, ...) _warn("[E] " fmt, ## __VA_ARGS__)
#define log_w(fmt, ...) _warn("[W] " fmt, ## __VA_ARGS__)
#define log_i(fmt, ...) _warn("[I] " fmt, ## __VA_ARGS__)
#define log_d(fmt, ...) _warn("[D] " fmt, ## __VA_ARGS__)

#ifdef DEBUG
#define log_t(fmt, ...) _warn("[T] " fmt, ## __VA_ARGS__)
#else
#define log_t(...)
#endif

void process_log_queue(void *arg __attribute__((unused)));
void push_log(const char *str, int str_len);
void destroy_log();

#endif
