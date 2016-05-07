#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <pthread.h>

#include "tuple_space.h"

#include "tarantool/tarantool.h"
#include "tarantool/tnt_net.h"
#include "tarantool/tnt_opt.h"

#include "msgpuck/msgpuck.h"

#include "threadpool.h"

#define LUA_in_FUNC "tuple_space.get_tuple"
#define LUA_rd_FUNC "tuple_space.read_tuple"
#define LUA_out_FUNC "tuple_space.add_tuple"

#define TUPLE_SPACE_COMM_T(t) tuple_space_comm_ ## t ## _type
#define TUPLE_SPACE_FORM_T(t) tuple_space_form_ ## t ## _type
#define TUPLE_SPACE_ARR_T(t)  tuple_space_arr_  ## t ## _type
#define TUPLE_SPACE_FARR_T(t) tuple_space_farr_ ## t ## _type

#define TUPLE_SPACE_ARR_VT(t)  tuple_space_arr_  ## t ## _type_t
#define TUPLE_SPACE_FARR_VT(t) tuple_space_farr_ ## t ## _type_t

#define TUPLE_SPACE_COMM_V(t) tuple_space_comm_ ## t ## _val
#define TUPLE_SPACE_FORM_V(t) tuple_space_form_ ## t ## _val
#define TUPLE_SPACE_ARR_V(t)  tuple_space_arr_  ## t ## _val
#define TUPLE_SPACE_FARR_V(t) tuple_space_farr_ ## t ## _val

#define PRAGMA_TYPE(n) tuple_space_pragma_ ## n

enum tuple_space_pragma_type_t {
#	define HELPER(n) PRAGMA_TYPE(n),
	TUPLE_SPACE_SUPPORTED_PRAGMAS(HELPER)

	tuple_space_max_auto_pragma_type,

	HELPER(eval)
#	undef HELPER

	tuple_space_max_pragma_type
};

enum tuple_space_arg_type_t {
#	define COMM(t, n, ...)		TUPLE_SPACE_COMM_T(n),
#	define FORM(t, n, ...)		TUPLE_SPACE_FORM_T(n),
#	define ARR(t, n, ...)		TUPLE_SPACE_ARR_T(n),
#	define FORM_ARR(t, n, ...)	TUPLE_SPACE_FARR_T(n),

	TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#	undef COMM
#	undef FORM
#	undef ARR
#	undef FORM_ARR
};

struct tuple_space_expr_t {
#	define COMM(t, n, ...) const t TUPLE_SPACE_COMM_V(n);
#	define FORM(t, n, ...) t *TUPLE_SPACE_FORM_V(n);
#	define ARR(t, n, ...) struct TUPLE_SPACE_ARR_VT(n) {					\
		unsigned s;									\
		t const* v;									\
	} TUPLE_SPACE_ARR_V(n);
#	define FORM_ARR(t, n, ...) struct TUPLE_SPACE_FARR_VT(n) {				\
		unsigned s;									\
		t *v;										\
	} TUPLE_SPACE_FARR_V(n);

	enum tuple_space_arg_type_t type;
	union {
		TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)
	};

#	undef COMM
#	undef FORM
#	undef ARR
#	undef FORM_ARR
};

#define DECL_RET_VAR(n)										\
	struct tuple_space_expr_t *n = (struct tuple_space_expr_t *)calloc(1, sizeof(struct tuple_space_expr_t));

#define COMM(t, n, ...)										\
	TUPLE_SPACE_COMMON_F_NAME(t, n) {							\
		DECL_RET_VAR(ret);								\
		*ret = (struct tuple_space_expr_t){						\
			.type = TUPLE_SPACE_COMM_T(n),						\
			.TUPLE_SPACE_COMM_V(n) = arg,						\
		};										\
		return ret;									\
	}

#define FORM(t, n, ...)										\
	TUPLE_SPACE_FORMAL_F_NAME(t, n) {							\
		DECL_RET_VAR(ret);								\
		*ret = (struct tuple_space_expr_t){						\
			.type = TUPLE_SPACE_FORM_T(n),						\
			.TUPLE_SPACE_FORM_V(n) = arg,						\
		};										\
		return ret;									\
	}

#define ARR(t, n, ...)										\
	TUPLE_SPACE_ARR_F_NAME(t, n) {								\
		DECL_RET_VAR(ret);								\
		*ret = (struct tuple_space_expr_t){						\
			.type = TUPLE_SPACE_ARR_T(n),						\
			.TUPLE_SPACE_ARR_V(n) = (struct TUPLE_SPACE_ARR_VT(n)){			\
				.s = size,							\
				.v = arg,							\
			},									\
		};										\
		return ret;									\
	}

#define FORM_ARR(t, n, ...)									\
	TUPLE_SPACE_FORMAL_ARR_F_NAME(t, n) {							\
		DECL_RET_VAR(ret);								\
		*ret = (struct tuple_space_expr_t){						\
			.type = TUPLE_SPACE_FARR_T(n),						\
			.TUPLE_SPACE_FARR_V(n) = (struct TUPLE_SPACE_FARR_VT(n)){		\
				.s = size,							\
				.v = arg,							\
			},									\
		};										\
		return ret;									\
	}

TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR

struct tuple_space_configuration_t {
	char host[255];
	uint16_t port;

	struct timeval conn_timeout;
	struct timeval req_timeout;

	threadpool_t *thread_pool;
	pthread_key_t threads_key; // key, shared by all threads. Used to get thread-specific data from storage
};

static struct tuple_space_configuration_t tuple_space_configuration;

struct thread_data_t;

struct tuple_space_processor_t {
	struct tuple_space_expr_t **args;
	unsigned n_args;

	struct thread_data_t *this_task; // This needed to get current task name and
					 // current thread configuration
};

struct single_thread_t {
	struct tnt_stream *tnt;
	struct tnt_request *request;
	struct tnt_reply *reply;
	struct tnt_stream *stream;

	struct thread_data_t *current_eval; // XXX: Don't destroy this object
};

struct thread_data_t {
	tuple_space_cb_t cb;
	void *arg;
	char name[512];
	int ret;

	struct single_thread_t *thread;
};

static void destroy_thread_data(struct thread_data_t *thd) {
	thd->thread->current_eval = NULL; // current thread haven't got a task
	free(thd);
}

#ifdef DEBUG

#ifndef MAX_STR_VALUE_LEN
#	define MAX_STR_VALUE_LEN 4096
#endif

__attribute__((nonnull))
static void dump_expr(const char *prefix, const struct tuple_space_expr_t *expr) {
	const char *expr_type = "unknown";
	char expr_value[MAX_STR_VALUE_LEN];
	bool is_formal = false;

	switch (expr->type) {
#define COMM(t, n, f, ...)									\
		case TUPLE_SPACE_COMM_T(n): {							\
			expr_type = STR(TUPLE_SPACE_COMM_T(n));					\
			snprintf(expr_value, sizeof(expr_value), f, expr->TUPLE_SPACE_COMM_V(n));\
			break;									\
		}
#define FORM(t, n, f, ...)									\
		case TUPLE_SPACE_FORM_T(n): {							\
			expr_type = STR(TUPLE_SPACE_FORM_T(n));					\
			is_formal = true;							\
			break;									\
		}
#define ARR(t, n, f, ...)									\
		case TUPLE_SPACE_ARR_T(n): {							\
			expr_type = STR(TUPLE_SPACE_ARR_T(n));					\
			int printed = snprintf(expr_value, sizeof(expr_value),			\
					"count: %u, data: (", expr->TUPLE_SPACE_ARR_V(n).s);	\
			for (int i = 0;	printed < sizeof(expr_value)				\
					&& i < expr->TUPLE_SPACE_ARR_V(n).s			\
					&& (expr->type != TUPLE_SPACE_ARR_T(char)		\
						|| expr->TUPLE_SPACE_ARR_V(n).v[i])		\
					; ++i) {						\
				printed += snprintf(expr_value + printed,			\
						sizeof(expr_value) - printed,			\
						f "%s",						\
						expr->TUPLE_SPACE_ARR_V(n).v[i],		\
						(i == expr->TUPLE_SPACE_ARR_V(n).s - 1 ? "" : ", "));\
			}									\
			if (printed < sizeof(expr_value))					\
				snprintf(expr_value + printed, sizeof(expr_value) - printed, ")");\
			break;									\
		}
#define FORM_ARR(t, n, f, ...)									\
		case TUPLE_SPACE_FARR_T(n): {							\
			expr_type = STR(TUPLE_SPACE_FARR_T(n));					\
			snprintf(expr_value, sizeof(expr_value), "count: %u", expr->TUPLE_SPACE_FARR_V(n).s);\
			break;									\
		}

		TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR
	};

	log_t("%s%s%s%s", prefix, expr_type, is_formal ? "" : " => ", is_formal ? "" : expr_value);
}

__attribute__((nonnull))
static void dump_pragma(enum tuple_space_pragma_type_t pragma_type, const struct tuple_space_processor_t *pragma) {
	static const char *pragmas_names[] = {
#		define HELPER(n) [PRAGMA_TYPE(n)] = #n,
		TUPLE_SPACE_SUPPORTED_PRAGMAS(HELPER)
		HELPER(eval)
#		undef HELPER
	};

	const char *pragma_name = pragmas_names[pragma_type];

	log_t("%s(", pragma_name);
	for (unsigned i = 0; i < pragma->n_args; ++i) {
		dump_expr("\t", pragma->args[i]);
	}
	log_t(")");
}

#else
#	define dump_expr(...)
#	define dump_pragma(...)
#endif // DEBUG

__attribute__((nonnull))
inline static struct tnt_stream *get_tnt(const struct tuple_space_processor_t *prc) {
	assert(prc->this_task && prc->this_task->thread);
	return prc->this_task->thread->tnt;
}

__attribute__((nonnull))
inline static struct tnt_request *get_req(const struct tuple_space_processor_t *prc) {
	assert(prc->this_task && prc->this_task->thread);
	return prc->this_task->thread->request;
}

__attribute__((nonnull))
inline static uint32_t get_req_sync(const struct tuple_space_processor_t *prc) {
	struct tnt_request *req = get_req(prc);
	assert(req);

	return req->hdr.sync;
}

inline static struct tnt_stream *get_stream(const struct tuple_space_processor_t *prc) {
	assert(prc->this_task && prc->this_task->thread);
	return prc->this_task->thread->stream;
}

__attribute__((nonnull))
static void tuple_space_pragma_cleanup(struct tuple_space_processor_t *prc) {
	for (int i = 0; i < prc->n_args; ++i) {
		free(prc->args[i]);
	}
	free(prc->args);

	memset(prc, 0, sizeof(*prc)); // don't free prc: allocated on stack
}

__attribute__((nonnull))
static bool tuple_space_process_in_pragma(struct tuple_space_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static bool tuple_space_process_out_pragma(struct tuple_space_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static bool tuple_space_process_rd_pragma(struct tuple_space_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static void tuple_space_tuple_add_char(struct tnt_stream *e, char c) {
	tnt_object_add_int(e, c);
}

__attribute__((nonnull))
static void tuple_space_tuple_add_int(struct tnt_stream *e, int v) {
	tnt_object_add_int(e, v);
}

__attribute__((nonnull))
inline static void tuple_space_tuple_add_double(struct tnt_stream *e, double v) {
	tnt_object_add_double(e, v);
}

__attribute__((nonnull))
inline static void tuple_space_tuple_add_char_ptrnz(struct tnt_stream *e, const char *ptr, unsigned size) {
	tnt_object_add_str(e, ptr, size);
}

__attribute__((nonnull))
inline static void tuple_space_tuple_add_char_ptr(struct tnt_stream *e, const char *ptr) {
	return tuple_space_tuple_add_char_ptrnz(e, ptr, strlen(ptr));
}

__attribute__((nonnull))
inline static bool is_formal_scalar(struct tuple_space_expr_t *e) {
#	define X(t, n, ...) || (e->type == TUPLE_SPACE_FORM_T(n))
	return false TUPLE_SPACE_SUPPORTED_TYPES(X);
#	undef X
}

__attribute__((nonnull))
inline static bool is_formal_array(struct tuple_space_expr_t *e) {
#	define X(t, n, ...) || (e->type == TUPLE_SPACE_FARR_T(n))
	return false TUPLE_SPACE_SUPPORTED_TYPES(X);
#	undef X
}

__attribute__((nonnull))
inline static bool is_formal(struct tuple_space_expr_t *e) {
	return is_formal_scalar(e) || is_formal_array(e);
}

__attribute__((nonull))
inline static bool is_array(struct tuple_space_expr_t *e) {
#	define X(t, n, ...) || (e->type == TUPLE_SPACE_ARR_T(n))
	return is_formal_array(e) TUPLE_SPACE_SUPPORTED_TYPES(X);
#	undef X
}

__attribute__((nonnull))
static void tuple_space_init_stream_expr(struct tnt_stream *s, struct tuple_space_expr_t *e) {
	tnt_object_add_array(s, is_array(e) ? 3 : 2);
	const char *name = NULL;
	const char *__typename = NULL;

	if (e->type == TUPLE_SPACE_ARR_T(char)) {
		// array of characters is the same as a string here
		name = "char_ptr"; // just a hack
	} else {
		switch (e->type) {
#		define COMM(t, n, ...) case TUPLE_SPACE_COMM_T(n): name = #n; break;
#		define FORM(t, n, ...) case TUPLE_SPACE_FORM_T(n): name = "formal"; __typename = #n; break;
#		define ARR(t, n, ...)  case TUPLE_SPACE_ARR_T(n):  name = "array_" #n; break;
#		define FARR(t, n, ...) case TUPLE_SPACE_FARR_T(n): name = "formal"; __typename = "array_" #n; break;

			TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FARR)
			default: break;

#		undef COMM
#		undef FORM
#		undef ARR
#		undef FARR
		};
	}

	assert(name);

	tnt_object_add_strz(s, name);

	if (is_formal(e)) {
		assert(__typename);
		tnt_object_add_strz(s, __typename);
	}
}

__attribute__((nonnull))
static void tuple_space_mk_request_args(struct tuple_space_processor_t *prc) {
	struct tnt_stream *tuple = get_stream(prc);

	tnt_object_add_array(tuple, prc->n_args);
	for (int i = 0; i < prc->n_args; ++i) {
		// array of 2 items will be generated here. First item is a name of an object.
		// Second element is object value. First element will be initialized into function above.
		struct tuple_space_expr_t *e = prc->args[i];
		tuple_space_init_stream_expr(tuple, e);

		if (e->type == TUPLE_SPACE_ARR_T(char)) {
			// array of characters is the same as a string here
			// check string is null-terminated
			unsigned n = 0;
			if (e->TUPLE_SPACE_ARR_V(char).v[e->TUPLE_SPACE_ARR_V(char).s - 1] == '\0')
				n = 1;

			tuple_space_tuple_add_char_ptrnz(tuple, e->TUPLE_SPACE_ARR_V(char).v, e->TUPLE_SPACE_ARR_V(char).s - n);
			continue;
		}

#		define COMM(t, n, ...)								\
			case TUPLE_SPACE_COMM_T(n):						\
				tuple_space_tuple_add_ ## n(tuple, e->TUPLE_SPACE_COMM_V(n));	\
				break;
#		define FORM(t, n, ...) case TUPLE_SPACE_FORM_T(n): break; /* nothing to do */
#		define ARR(t, n, ...)								\
			case TUPLE_SPACE_ARR_T(n): {						\
				tnt_object_add_int(tuple, (int64_t)e->TUPLE_SPACE_ARR_V(n).s);	\
				tnt_object_add_array(tuple, e->TUPLE_SPACE_ARR_V(n).s);		\
				for (int i = 0; i < e->TUPLE_SPACE_ARR_V(n).s; ++i) {		\
					tuple_space_tuple_add_ ## n(tuple, e->TUPLE_SPACE_ARR_V(n).v[i]); \
				}								\
				break;								\
			}
#		define FARR(t, n, ...)								\
			case TUPLE_SPACE_FARR_T(n):						\
				tnt_object_add_int(tuple, (int64_t)e->TUPLE_SPACE_FARR_V(n).s);	\
				break;

		switch (e->type) {
			TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FARR)
			default:
				assert(!"Unexpected expression found");
		}

#		undef COMM
#		undef FORM
#		undef ARR
#		undef FARR

	}
}

static struct single_thread_t *mk_thread_data() {
	struct single_thread_t *th =
		(struct single_thread_t *)calloc(1, sizeof(struct single_thread_t));
	assert(th);

	pthread_setspecific(tuple_space_configuration.threads_key, th);
	return th;
}

static int tuple_space_connect(struct single_thread_t *thread_conf);
static struct single_thread_t *get_thread_conf() {
	struct single_thread_t *conf = pthread_getspecific(tuple_space_configuration.threads_key);
	if (!conf) {
		conf = mk_thread_data();
		tuple_space_connect(conf);
	}

	return conf;
}

__attribute__((nonnull))
static bool tuple_space_send_request_to_tarantool(const char *lua_func_name, const struct tuple_space_processor_t *prc) {
	log_i("Trying to call %s tarantool's function", lua_func_name);

	struct tnt_stream *tnt = get_tnt(prc);
	assert(tnt);

	struct tnt_request *req = prc->this_task->thread->request
		= tnt_request_call(prc->this_task->thread->request);
	assert(req);

	if (tnt_request_set_funcz(req, lua_func_name) == -1) {
		log_e("Can't set function name: %s", lua_func_name);
		return false;
	}

	if (tnt_request_set_tuple(req, get_stream(prc)) == -1) {
		log_e("Can't set function args: %s", lua_func_name);
		return false;
	}

	if (tnt_request_compile(tnt, req) == -1) {
		log_e("Can't compile tnt request, func name: %s, error: %s", lua_func_name, tnt_strerror(tnt));
		return false;
	}

	log_i("Function %s was called, sync = %u", lua_func_name, get_req_sync(prc));

	return true;
}

__attribute__((nonnull))
static bool tuple_space_process_pragma(enum tuple_space_pragma_type_t pragma_type, struct tuple_space_processor_t *prc) {
	assert(pragma_type >= 0 && pragma_type < tuple_space_max_auto_pragma_type);

	dump_pragma(pragma_type, prc);

	struct single_thread_t *th = get_thread_conf();
	assert(th && th->current_eval && th->current_eval->thread);

	prc->this_task = th->current_eval;

	struct local_processor_t {
		const char *lua_func_name;
		bool (*processor)(struct tuple_space_processor_t *);
	} pragma_processors[] = {
#		define HELPER(n)								\
		[PRAGMA_TYPE(n)] = {								\
			.lua_func_name = LUA_ ## n ## _FUNC,					\
			.processor = tuple_space_process_ ## n ## _pragma,			\
		},

		TUPLE_SPACE_SUPPORTED_PRAGMAS(HELPER)

#		undef HELPER
	};

	struct local_processor_t *processor = &pragma_processors[pragma_type];

	prc->this_task->thread->stream = tnt_object(prc->this_task->thread->stream);
	assert(prc->this_task->thread->stream);
	tuple_space_mk_request_args(prc);

	// send request into tarantool
	tuple_space_send_request_to_tarantool(processor->lua_func_name, prc);

	// parse tarantool response
	bool ret = processor->processor(prc);

	tuple_space_pragma_cleanup(prc);
	return ret;
}

#define MK_PRAGMA_PROCESSOR(name)								\
	__attribute__((nonnull))								\
	void TUPLE_SPACE_PRAGMA_PROCESSOR(name)(int *ret, unsigned n_args, ...) {		\
		assert(ret);									\
		*ret = 1;									\
												\
		log_d("Trying to process %s pragma, n_args == %u", #name, n_args);		\
												\
												\
		if (!n_args)									\
			return;									\
												\
		struct tuple_space_processor_t prc;						\
		memset(&prc, 0, sizeof(prc));							\
												\
		prc.n_args = n_args;								\
		prc.args = (struct tuple_space_expr_t **)calloc(n_args, sizeof(struct tuple_space_expr_t *));\
												\
		assert(prc.n_args);								\
												\
		va_list ap;									\
		va_start (ap, n_args);								\
												\
		for (unsigned i = 0; i < n_args; ++i) {						\
			log_t("Processing %d arg from %d", i, n_args);				\
			prc.args[i] = va_arg(ap, struct tuple_space_expr_t *);			\
			assert(prc.args[i]);							\
		}										\
												\
		va_end(ap);									\
		*ret = tuple_space_process_pragma(PRAGMA_TYPE(name), &prc);			\
	}

TUPLE_SPACE_SUPPORTED_PRAGMAS(MK_PRAGMA_PROCESSOR)

#undef MK_PRAGMA_PROCESSOR

__attribute__((destructor))
static void tuple_space_destroy() {
	log_d("Trying to destroy thread pool...");
	destroy_log();
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;
	threadpool_destroy(conf->thread_pool, threadpool_graceful);
	log_d("Thread pool successfully destroyed");
}

static void tuple_space_destroy_cur_thread_data(struct single_thread_t *conf) {
	if (!conf)
		return;

	if (conf->request)
		tnt_request_free(conf->request);

	if (conf->reply)
		tnt_reply_free(conf->reply);

	if (conf->tnt) {
		tnt_close(conf->tnt);
		tnt_stream_free(conf->tnt);
	}

	if (conf->stream)
		tnt_stream_free(conf->stream);

	free(conf);
	pthread_setspecific(tuple_space_configuration.threads_key, NULL);
}

static int tuple_space_ping(struct single_thread_t *thread_conf) {
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;
	log_w("Sending ping to %s:%d...", conf->host, conf->port);

	tnt_ping(thread_conf->tnt);
	thread_conf->reply = tnt_reply_init(thread_conf->reply);

	if (!thread_conf->reply) {
		log_e("Can't create reply for ping request: %s", tnt_strerror(thread_conf->tnt));
		return -1;
	}

	if (thread_conf->tnt->read_reply(thread_conf->tnt, thread_conf->reply) == -1) {
		log_e("Can't get reply for ping request: %s", tnt_strerror(thread_conf->tnt));
		return -1;
	}

	log_w("Ping done");
	return 0;
}

static int tuple_space_connect(struct single_thread_t *thread_conf) {
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;

	bool reconnecting = false;
	if (thread_conf->tnt) {
		log_w("Reconnecting to tuple space...");
		reconnecting = true;
	} else {
		log_w("Trying to connect to %s:%u", conf->host, conf->port);
	}

	thread_conf->tnt = tnt_net(thread_conf->tnt);

	if (!thread_conf->tnt) {
		log_e("Can't create stream: %s", tnt_strerror(thread_conf->tnt));
		return -1;
	}

	if (!reconnecting) {
		// otherwise args will be copyed from previous version of tnt_stream
		char uri[sizeof(conf->host) + sizeof("99999")] = "";
		snprintf(uri, sizeof(uri), "%s:%u", conf->host, conf->port);

		tnt_set(thread_conf->tnt, TNT_OPT_URI, uri);
		tnt_set(thread_conf->tnt, TNT_OPT_SEND_BUF, 0); // disable buffering for send
		tnt_set(thread_conf->tnt, TNT_OPT_RECV_BUF, 0); // disable buffering for recv

		tnt_set(thread_conf->tnt, TNT_OPT_TMOUT_CONNECT, &conf->conn_timeout);
		tnt_set(thread_conf->tnt, TNT_OPT_TMOUT_SEND, &conf->req_timeout);
	}

	if (tnt_connect(thread_conf->tnt) == -1) {
		log_e("Can't connect to tuple space (%s:%u): %s",
				conf->host, conf->port, tnt_strerror(thread_conf->tnt));
		return -1;
	}

	log_w("Connected to %s:%u", conf->host, conf->port);
	return tuple_space_ping(thread_conf);
}

__attribute__((nonnull))
static void set_thread_cross_pointers(struct thread_data_t *thread_data, struct single_thread_t *th) {
	assert(!th->current_eval);

	thread_data->thread = th;
	th->current_eval = thread_data;
}

static void preprocess_thread(struct thread_data_t *thread_data) {
	// TODO:
	//	* mark all eval statements
	//	* push all callbacks into vector at startup
	//		* generate list of evals in compile time
	//	* push tuple with current eval identificator into tuple space
	//		and share task over all processes
}

static void postprocess_thread(struct thread_data_t *thread_data) {
	// TODO:
	//	push retcode into tuple space. XXX: don't forget about arg
	//	If retcode is 0, push 0 and wait for pending tasks.
	//	Otherwise, stop all other tasks of this type.
}

static bool check_eval_task(struct thread_data_t *thread_data) {
	// TODO:
	//	* check current task is exists in tuple space
	//	* if not, finish current thread.
	//	* start processing otherwise
	//
	// TODO:
	//	* create a process which polling a tuple space to find new tasks to
	//		execute.
	//	* If the task is found, evaluate it.
	return true;
}

static void on_thread_create(struct thread_data_t *thread_data) {
	log_d("Evaluation with name %s was started", thread_data->name);

	struct single_thread_t *conf = get_thread_conf();
	assert(conf);

	set_thread_cross_pointers(thread_data, conf);

	bool task_found = check_eval_task(thread_data);
	if (task_found) {
		thread_data->ret = thread_data->cb(thread_data->arg); // TODO: process ret value !!!
		postprocess_thread(thread_data);
	}

	log_d("Evaluation with name %s was done, ret == %d", thread_data->name, thread_data->ret);

	destroy_thread_data(thread_data);
}

__attribute__((constructor))
static void init_pool() {
	log_d("Trying to create thread pool, n_threads == %d, queue_size == %d", N_THREADS, QUEUE_SIZE);

	tuple_space_configuration.thread_pool = threadpool_create(N_THREADS, QUEUE_SIZE, 0);
	threadpool_add(tuple_space_configuration.thread_pool, (void (*)(void *))process_log_queue, NULL, 0);

	log_d("Thread pool created");
}

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout) {
	int printed = snprintf(tuple_space_configuration.host, sizeof(tuple_space_configuration.host), "%s", host);
	if (printed >= sizeof(tuple_space_configuration.host)) {
		log_e("Too long hostname given into tuple_space_set_configuration()");
		return -1;
	}

	// initialize thread key
	pthread_key_create(&tuple_space_configuration.threads_key, (void (*)(void *))&tuple_space_destroy_cur_thread_data);

	struct timeval default_conn_timeout = { .tv_sec = 1, .tv_usec = 0,  },
		       default_req_timeout = { .tv_sec = 0, .tv_usec = 500, };

	if (!conn_timeout)
		conn_timeout = &default_conn_timeout;
	if (!req_timeout)
		req_timeout = &default_req_timeout;

	memcpy(&tuple_space_configuration.conn_timeout, conn_timeout, sizeof(*conn_timeout));
	memcpy(&tuple_space_configuration.req_timeout, req_timeout, sizeof(*req_timeout));

	tuple_space_configuration.port = port;

	struct thread_data_t *thread_data = (struct thread_data_t *)calloc(1, sizeof(struct thread_data_t));
	assert(thread_data);

	snprintf(thread_data->name, sizeof(thread_data->name), "__main_thread__");
	preprocess_thread(thread_data);

	struct single_thread_t *conf = get_thread_conf(); // init main thread
	set_thread_cross_pointers(thread_data, conf);

	return 0;
}

int TUPLE_SPACE_PRAGMA_PROCESSOR(eval)(const char *name, tuple_space_cb_t cb, void *arg) {
	assert(name);

	log_d("Trying to eval tuple with name %s", name);

	struct thread_data_t *thread_data = (struct thread_data_t *)calloc(1, sizeof(struct thread_data_t));
	assert(thread_data);

	thread_data->cb = cb;
	thread_data->arg = arg;

	snprintf(thread_data->name, sizeof(thread_data->name), "%s", name);

	preprocess_thread(thread_data);
	threadpool_add(tuple_space_configuration.thread_pool, (void (*)(void *))on_thread_create, thread_data, 0);

	log_d("Eval stmt was pushed into queue");

	return 0;
}

