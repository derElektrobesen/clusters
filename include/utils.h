#ifndef UTILS_H
#define UTILS_H

#define MANAGER_RANK 0
#define MANAGER_STR "MANAGER"
#define WORKER_STR "WORKER"

#define warn(fmt, ...) ({						\
		if (get_rank() == MANAGER_RANK) {			\
			u_printf(MANAGER_STR, fmt, ##__VA_ARGS__);	\
		} else {						\
			u_printf(WORKER_STR, fmt, ##__VA_ARGS__);	\
		}							\
	})

#define SV_SIZE(v) (sizeof(v) / sizeof(*(v)))

void set_rank(short rank);
short get_rank();

void u_printf(const char *type, const char *fmt, ...)
	__attribute__((format(printf, 2, 3)));

#endif
