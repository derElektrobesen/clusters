#include <stdarg.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "tuple_space.h"

#define TNT_COMM_T(t) tnt_comm_ ## t ## _type
#define TNT_FORM_T(t) tnt_form_ ## t ## _type
#define TNT_ARR_T(t)  tnt_arr_  ## t ## _type
#define TNT_FARR_T(t) tnt_farr_ ## t ## _type

#define TNT_ARR_VT(t)  tnt_arr_  ## t ## _type_t
#define TNT_FARR_VT(t) tnt_farr_ ## t ## _type_t

#define TNT_COMM_V(t) tnt_comm_ ## t ## _val
#define TNT_FORM_V(t) tnt_form_ ## t ## _val
#define TNT_ARR_V(t)  tnt_arr_  ## t ## _val
#define TNT_FARR_V(t) tnt_farr_ ## t ## _val

enum tnt_arg_type_t {
#	define COMM(t, n, ...)		TNT_COMM_T(n),
#	define FORM(t, n, ...)		TNT_FORM_T(n),
#	define ARR(t, n, ...)		TNT_ARR_T(n),
#	define FORM_ARR(t, n, ...)	TNT_FARR_T(n),

	TNT_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#	undef COMM
#	undef FORM
#	undef ARR
#	undef FORM_ARR
};

struct tnt_expr_t {
#	define COMM(t, n, ...) const t TNT_COMM_V(n);
#	define FORM(t, n, ...) t *TNT_FORM_V(n);
#	define ARR(t, n, ...) struct TNT_ARR_VT(n) {						\
		unsigned s;									\
		t const* v;									\
	} TNT_ARR_V(n);
#	define FORM_ARR(t, n, ...) struct TNT_FARR_VT(n) {					\
		unsigned s;									\
		t *v;										\
	} TNT_FARR_V(n);

	enum tnt_arg_type_t type;
	union {
		TNT_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)
	};

#	undef COMM
#	undef FORM
#	undef ARR
#	undef FORM_ARR
};

#define DECL_RET_VAR(n)										\
	struct tnt_expr_t *n = (struct tnt_expr_t *)calloc(1, sizeof(struct tnt_expr_t));

#define COMM(t, n, ...)										\
	TNT_COMMON_F_NAME(t, n) {								\
		DECL_RET_VAR(ret);								\
		*ret = (struct tnt_expr_t){							\
			.type = TNT_COMM_T(n),							\
			.TNT_COMM_V(n) = arg,							\
		};										\
		return ret;									\
	}

#define FORM(t, n, ...)										\
	TNT_FORMAL_F_NAME(t, n) {								\
		DECL_RET_VAR(ret);								\
		*ret = (struct tnt_expr_t){							\
			.type = TNT_FORM_T(n),							\
			.TNT_FORM_V(n) = arg,							\
		};										\
		return ret;									\
	}

#define ARR(t, n, ...)										\
	TNT_ARR_F_NAME(t, n) {									\
		DECL_RET_VAR(ret);								\
		*ret = (struct tnt_expr_t){							\
			.type = TNT_ARR_T(n),							\
			.TNT_ARR_V(n) = (struct TNT_ARR_VT(n)){					\
				.s = size,							\
				.v = arg,							\
			},									\
		};										\
		return ret;									\
	}

#define FORM_ARR(t, n, ...)									\
	TNT_FORMAL_ARR_F_NAME(t, n) {								\
		DECL_RET_VAR(ret);								\
		*ret = (struct tnt_expr_t){							\
			.type = TNT_FARR_T(n),							\
			.TNT_FARR_V(n) = (struct TNT_FARR_VT(n)){				\
				.s = size,							\
				.v = arg,							\
			},									\
		};										\
		return ret;									\
	}

TNT_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR

struct tnt_processor_t {
	struct tnt_expr_t **args;
	unsigned n_args;
};

#ifdef DEBUG

#ifndef MAX_STR_VALUE_LEN
#	define MAX_STR_VALUE_LEN 4096
#endif

__attribute__((nonnull))
static void tnt_pragma_cleanup(struct tnt_processor_t *prc) {
	for (int i = 0; i < prc->n_args; ++i) {
		free(prc->args[i]);
	}
	free(prc->args);

	memset(prc, 0, sizeof(*prc));
}

__attribute__((nonnull))
static void DUMP_EXPR(const char *prefix, const struct tnt_expr_t *expr) {
	const char *expr_type = "unknown";
	char expr_value[MAX_STR_VALUE_LEN];
	bool is_formal = false;

	switch (expr->type) {
#define COMM(t, n, f, ...)									\
		case TNT_COMM_T(n): {								\
			expr_type = STR(TNT_COMM_T(n));						\
			snprintf(expr_value, sizeof(expr_value), f, expr->TNT_COMM_V(n));	\
			break;									\
		}
#define FORM(t, n, f, ...)									\
		case TNT_FORM_T(n): {								\
			expr_type = STR(TNT_FORM_T(n));						\
			is_formal = true;							\
			break;									\
		}
#define ARR(t, n, f, ...)									\
		case TNT_ARR_T(n): {								\
			expr_type = STR(TNT_ARR_T(n));						\
			int printed = snprintf(expr_value, sizeof(expr_value),			\
					"count: %u, data: (", expr->TNT_ARR_V(n).s);		\
			for (int i = 0;	printed < sizeof(expr_value)				\
					&& i < expr->TNT_ARR_V(n).s				\
					&& (expr->type != TNT_ARR_T(char)			\
						|| expr->TNT_ARR_V(n).v[i])			\
					; ++i) {						\
				printed += snprintf(expr_value + printed,			\
						sizeof(expr_value) - printed,			\
						f "%s",						\
						expr->TNT_ARR_V(n).v[i],			\
						(i == expr->TNT_ARR_V(n).s - 1 ? "" : ", "));	\
			}									\
			if (printed < sizeof(expr_value))					\
				snprintf(expr_value + printed, sizeof(expr_value) - printed, ")"); \
			break;									\
		}
#define FORM_ARR(t, n, f, ...)									\
		case TNT_FARR_T(n): {								\
			expr_type = STR(TNT_FARR_T(n));						\
			snprintf(expr_value, sizeof(expr_value), "count: %u", expr->TNT_FARR_V(n).s); \
			break;									\
		}

		TNT_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)

#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR
	};

	log_t("%s%s%s%s", prefix, expr_type, is_formal ? "" : " => ", is_formal ? "" : expr_value);
}

__attribute__((nonnull))
static void DUMP_PRAGMA(const char *pragma_name, const struct tnt_processor_t *pragma) {
	log_t("%s(", pragma_name);
	for (unsigned i = 0; i < pragma->n_args; ++i) {
		DUMP_EXPR("\t", pragma->args[i]);
	}
	log_t(")");
}

#else
#	define DUMP_EXPR(...)
#	define DUMP_PRAGMA(...)
#endif // DEBUG

__attribute__((nonnull))
static bool tnt_process_in_pragma(struct tnt_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static bool tnt_process_out_pragma(struct tnt_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static bool tnt_process_rd_pragma(struct tnt_processor_t *prc) {
	return true;
}

__attribute__((nonnull))
static bool tnt_process_eval_pragma(struct tnt_processor_t *prc) {
	return true;
}

#define MK_PRAGMA_PROCESSOR(name)								\
	__attribute__((nonnull))								\
	bool TNT_PRAGMA_PROCESSOR(name)(unsigned n_args, ...) {					\
		if (!n_args)									\
			return true;								\
												\
		struct tnt_processor_t prc;							\
		memset(&prc, 0, sizeof(prc));							\
												\
		prc.n_args = n_args;								\
		prc.args = (struct tnt_expr_t **)calloc(n_args, sizeof(struct tnt_expr_t *));	\
												\
		assert(prc.n_args);								\
												\
		va_list ap;									\
		va_start (ap, n_args);								\
												\
		for (unsigned i = 0; i < n_args; ++i) {						\
			prc.args[i] = va_arg(ap, struct tnt_expr_t *);				\
			assert(prc.args[i]);							\
		}										\
												\
		va_end(ap);									\
		DUMP_PRAGMA(#name, &prc);							\
												\
		bool ret = tnt_process_ ## name ## _pragma(&prc);				\
		tnt_pragma_cleanup(&prc);							\
		return ret;									\
	}

TNT_SUPPORTED_PRAGMAS(MK_PRAGMA_PROCESSOR)

#undef MK_PRAGMA_PROCESSOR
