#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "tuple_space.h"

#include "tarantool/tarantool.h"
#include "tarantool/tnt_net.h"
#include "tarantool/tnt_opt.h"

#include "msgpuck/msgpuck.h"

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

struct tuple_space_processor_t {
	struct tuple_space_expr_t **args;
	unsigned n_args;

	struct tnt_stream *stream;
};

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
static void tuple_space_pragma_cleanup(struct tuple_space_processor_t *prc) {
	for (int i = 0; i < prc->n_args; ++i) {
		free(prc->args[i]);
	}
	free(prc->args);

	tnt_stream_free(prc->stream);

	memset(prc, 0, sizeof(*prc));
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
static bool tuple_space_process_eval_pragma(struct tuple_space_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static void tuple_space_tuple_add_char(struct tnt_stream *e, char c) {

}

__attribute__((nonnull))
static void tuple_space_tuple_add_int(struct tnt_stream *e, int v) {

}

__attribute__((nonnull))
static void tuple_space_tuple_add_double(struct tnt_stream *e, double v) {

}

__attribute__((nonnull))
static void tuple_space_tuple_add_char_ptrz(struct tnt_stream *e, const char *ptr, unsigned size) {

}

__attribute__((nonnull))
static void tuple_space_tuple_add_char_ptr(struct tnt_stream *e, const char *ptr) {
	return tuple_space_tuple_add_char_ptrz(e, ptr, strlen(ptr));
}

__attribute__((nonnull))
inline static bool is_formal_scalar(struct tuple_space_expr_t *e) {
#	define X(t, n, ...) || (e->type == TUPLE_SPACE_FORM_T(n))
	return false TUPLE_SPACE_SUPPORTED_TYPES(X);
#	undef X
}

__attribute__((nonnull))
static void tuple_space_init_stream_expr(struct tnt_stream *s, struct tuple_space_expr_t *e) {
	int n_args = is_formal_scalar(e) ? 1 : 2;

	tnt_object_add_array(s, n_args);
	const char *name = NULL;

#	define COMM(t, n, ...) case TUPLE_SPACE_COMM_T(n): name = #n; break;
#	define FORM(t, n, ...) case TUPLE_SPACE_FORM_T(n): name = "formal_" #n; break;
#	define ARR(t, n, ...)  case TUPLE_SPACE_ARR_T(n):  name = "array_" #n; break;
#	define FARR(t, n, ...) case TUPLE_SPACE_FARR_T(n): name = "formal_array_" #n; break;

	switch (e->type) {
		TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FARR)
		default: break;
	};

#	undef COMM
#	undef FORM
#	undef ARR
#	undef FARR

	assert(name);

	tnt_object_add_strz(s, name);
}

__attribute__((nonnull))
static void tuple_space_mk_stream(struct tuple_space_processor_t *prc) {
	assert(prc->stream);

	struct tnt_stream *tuple = prc->stream;

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

			tuple_space_tuple_add_char_ptrz(tuple, e->TUPLE_SPACE_ARR_V(char).v, e->TUPLE_SPACE_ARR_V(char).s - n);
			continue;
		}

#		define COMM(t, n, ...)								\
			case TUPLE_SPACE_COMM_T(n):						\
				tuple_space_tuple_add_ ## n(tuple, e->TUPLE_SPACE_COMM_V(n));	\
				break;
#		define FORM(t, n, ...) case TUPLE_SPACE_FORM_T(n): break; /* nothing to do */
#		define ARR(t, n, ...)								\
			case TUPLE_SPACE_ARR_T(n): {						\
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
	}
}

__attribute__((nonnull))
static bool tuple_space_process_pragma(enum tuple_space_pragma_type_t pragma_type, struct tuple_space_processor_t *prc) {
	assert(pragma_type < tuple_space_max_pragma_type);

	dump_pragma(pragma_type, prc);

	prc->stream = tnt_object(NULL);
	tuple_space_mk_stream(prc);

	bool ret = false;
	switch (pragma_type) {
#		define HELPER(n)								\
		case PRAGMA_TYPE(n):								\
			ret = tuple_space_process_ ## n ## _pragma(prc);			\
			break;

		TUPLE_SPACE_SUPPORTED_PRAGMAS(HELPER)

#		undef HELPER
		default:
			assert(!"Invalid pragma type found!");
	};

	tuple_space_pragma_cleanup(prc);
	return ret;
}

#define MK_PRAGMA_PROCESSOR(name)								\
	__attribute__((nonnull))								\
	bool TUPLE_SPACE_PRAGMA_PROCESSOR(name)(unsigned n_args, ...) {				\
		if (!n_args)									\
			return true;								\
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
			prc.args[i] = va_arg(ap, struct tuple_space_expr_t *);			\
			assert(prc.args[i]);							\
		}										\
												\
		va_end(ap);									\
		return tuple_space_process_pragma(PRAGMA_TYPE(name), &prc);			\
	}

TUPLE_SPACE_SUPPORTED_PRAGMAS(MK_PRAGMA_PROCESSOR)

#undef MK_PRAGMA_PROCESSOR

struct tuple_space_configuration_t {
	char host[255];
	uint16_t port;

	struct tnt_stream *tnt;
	struct tnt_reply *reply;
};

static struct tuple_space_configuration_t tuple_space_configuration;

__attribute__((destructor))
static void tuple_space_destroy() {
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;

	if (conf->reply)
		tnt_reply_free(conf->reply);

	if (conf->tnt) {
		tnt_close(conf->tnt);
		tnt_stream_free(conf->tnt);
	}
}

static int tuple_space_ping() {
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;
	log_i("Sending ping...");

	tnt_ping(conf->tnt);
	conf->reply = tnt_reply_init(conf->reply);

	if (!conf->reply) {
		log_e("Can't create reply for ping request: %s", tnt_strerror(conf->tnt));
		return -1;
	}

	if (conf->tnt->read_reply(conf->tnt, conf->reply) == -1) {
		log_e("Can't get reply for ping request: %s", tnt_strerror(conf->tnt));
		return -1;
	}

	log_i("Ping done");
	return 0;
}

static int tuple_space_connect(struct timeval *conn_timeout, struct timeval *req_timeout) {
	struct tuple_space_configuration_t *conf = &tuple_space_configuration;

	int reconnecting = 0;
	if (conf->tnt) {
		log_i("Reconnecting...");
		reconnecting = 1;
	}

	conf->tnt = tnt_net(conf->tnt);

	if (!conf->tnt) {
		log_e("Can't create stream: %s", tnt_strerror(conf->tnt));
		return -1;
	}

	if (!reconnecting) {
		// otherwise args will be copyed from previous version of tnt_stream
		char uri[sizeof(conf->host) + sizeof("99999")] = "";
		snprintf(uri, sizeof(uri), "%s:%u", conf->host, conf->port);

		tnt_set(conf->tnt, TNT_OPT_URI, uri);
		tnt_set(conf->tnt, TNT_OPT_SEND_BUF, 0); // disable buffering for send
		tnt_set(conf->tnt, TNT_OPT_RECV_BUF, 0); // disable buffering for recv

		struct timeval default_conn_timeout = { .tv_sec = 1, .tv_usec = 0,  },
			       default_req_timeout = { .tv_sec = 0, .tv_usec = 500, };

		if (!conn_timeout)
			conn_timeout = &default_conn_timeout;
		if (!req_timeout)
			req_timeout = &default_req_timeout;

		tnt_set(conf->tnt, TNT_OPT_TMOUT_CONNECT, conn_timeout);
		tnt_set(conf->tnt, TNT_OPT_TMOUT_SEND, req_timeout);
	}

	if (tnt_connect(conf->tnt) == -1) {
		log_e("Can't connect to tuple space (%s:%u): %s",
				conf->host, conf->port, tnt_strerror(conf->tnt));
		return -1;
	}

	log_i("Connected to %s:%u", conf->host, conf->port);
	return tuple_space_ping();
}

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout) {
	int printed = snprintf(tuple_space_configuration.host, sizeof(tuple_space_configuration.host), "%s", host);
	if (printed >= sizeof(tuple_space_configuration.host)) {
		log_e("Too long hostname given into tuple_space_set_configuration()");
		return -1;
	}

	tuple_space_configuration.port = port;
	return tuple_space_connect(conn_timeout, req_timeout);
}
