#include "utils.h"

#include <time.h>

#ifdef __MACH__ // fucking Machintosh
#include <mach/clock.h>
#include <mach/mach.h>

#include <pthread.h>
#include <stdbool.h>

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

#define log_real(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

static char log_data[5 Mb];
static bool can_write_log = false;

__thread char log_line[MAX_LOG_LINE];

static struct log_context_t {
	pthread_mutex_t lock;
	pthread_cond_t notify;

	struct log_context_queue_t {
		const char *ptr;
		int size;
	} queue[sizeof(log_data) / sizeof(log_line)];

	char *tail;
	int last_queue_n;
} log_context;

__attribute__((constructor(101))) // this constructor should be called first
static void init_log() {
	pthread_mutex_init(&log_context.lock, NULL);
	pthread_cond_init(&log_context.notify, NULL);
	log_context.tail = log_data;
}

__attribute__((destructor(101)))
static void deinit_log() {
	pthread_mutex_destroy(&log_context.lock);
	pthread_cond_destroy(&log_context.notify);
}

void process_log_queue(void *arg __attribute__((unused))) {
	can_write_log = true;

	// This function should be called called from thread pool
	while (can_write_log) {
		pthread_mutex_lock(&log_context.lock);
		pthread_cond_wait(&log_context.notify, &log_context.lock);

		for (int i = 0; i < log_context.last_queue_n; ++i) {
			log_real("%.*s", log_context.queue[i].size, log_context.queue[i].ptr);
		}

		log_context.last_queue_n = 0;
		log_context.tail = log_data;

		pthread_mutex_unlock(&log_context.lock);
	}
}

void push_log(const char *str, int size) {
	if (!can_write_log) {
		log_real("%.*s", size, str);
		return;
	}

	pthread_mutex_lock(&log_context.lock);

	char *tail = log_context.tail;
	if (log_data + sizeof(log_data) - tail < size
			|| log_context.last_queue_n == sizeof(log_context.queue) / sizeof(*log_context.queue)) {
		log_real("Too much logs!");
		pthread_mutex_unlock(&log_context.lock);
		return;
	}

	memcpy(tail, str, size);
	log_context.queue[log_context.last_queue_n++] = (struct log_context_queue_t){
		.ptr = tail,
		.size = size,
	};

	log_context.tail += size;

	pthread_cond_signal(&log_context.notify);
	pthread_mutex_unlock(&log_context.lock);
}

void destroy_log() {
	can_write_log = false;
	pthread_cond_signal(&log_context.notify);
}
