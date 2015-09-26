#include "utils.h"

#include <time.h>
#include <stdarg.h>
#include <stdio.h>
#include <assert.h>

static short _current_rank = -1;

void set_rank(short rank) {
	_current_rank = rank;
}

short get_rank() {
	assert(_current_rank != -1);
	return _current_rank;
}

void u_printf(const char *type, const char *fmt, ...) {
	// TODO: is the other way to do this exists?
	static __thread char str[1024] = "";

	va_list valist;
	va_start(valist, fmt);
	int printed = vsnprintf(str, sizeof(str), fmt, valist);
	va_end(valist);

	if (printed == sizeof(str) - 1) {
		warn("Possibly trying to print too many characters");
	}

	time_t rawtime = time(NULL);
	struct tm *timeinfo = localtime(&rawtime);

	printf("%s [%s] %s\n", asctime(timeinfo), type, str);
}
