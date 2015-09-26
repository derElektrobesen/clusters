#include "utils.h"

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

static const short _current_rank = -1;

void set_rank(short rank) {
	*(short *)&_current_rank = rank;
}

static void u_printf(const char *type, const char *fmt, va_list valist) {
	// TODO: is the other way to do this exists?
	static __thread char str[1024] = "";

	int printed = vsnprintf(str, sizeof(str), fmt, valist);
	if (printed == sizeof(str) - 1) {
		warn("Possibly trying to print too many characters");
	}

	time_t rawtime = time(NULL);
	struct tm *timeinfo = localtime(&rawtime);

	char str_time[64] = "";
	strftime(str_time, sizeof(str_time), "%d %b %T", timeinfo);

	printf("%s [%s] %s\n", str_time, type, str);
}

static void um_printf(const char *fmt, va_list valist) {
	u_printf(MANAGER_STR, fmt, valist);
}

static void uw_printf(short rank, const char *fmt, va_list valist) {
	char title[sizeof(WORKER_STR) + (2 << sizeof(rank)) + 1] = "";

	snprintf(title, sizeof(title), WORKER_STR ":%d", rank);
	u_printf(title, fmt, valist);
}

void warn(const char *fmt, ...) {
	va_list valist;
	va_start(valist, fmt);
	if (_current_rank == MANAGER_RANK)
		um_printf(fmt, valist);
	else
		uw_printf(_current_rank, fmt, valist);
	va_end(valist);
}
