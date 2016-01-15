#include "utils.h"

#include <time.h>

#ifdef __MACH__ // fucking Machintosh
#include <mach/clock.h>
#include <mach/mach.h>

void clock_gettime_impl(struct timespec *ts) {
	clock_serv_t cclock;
	mach_timespec_t mts;
	host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
	clock_get_time(cclock, &mts);
	mach_port_deallocate(mach_task_self(), cclock);
	ts->tv_sec = mts.tv_sec;
	ts->tv_nsec = mts.tv_nsec;
}

#else

void clock_gettime_impl(struct timespec *ts) {
	clock_gettime(CLOCK_REALTIME, ts);
}

#endif
