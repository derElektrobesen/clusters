#ifndef UTILS_H
#define UTILS_H

#define MANAGER_RANK 0
#define MANAGER_STR "MANAGER"
#define WORKER_STR "WORKER"

#define SV_SIZE(v) (sizeof(v) / sizeof(*(v)))

void set_rank(short rank);

void warn(const char *fmt, ...)
	__attribute__((format(printf, 1, 2)));

#endif
