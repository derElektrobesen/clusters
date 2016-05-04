#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <stdbool.h>
#include <utils.h>

#define TUPLE_SPACE_SUPPORTED_TYPES(_)				\
	_(char, char, "%c")				\
	_(int, int, "%d")				\
	_(double, double, "%lf")			\
	_(char *, char_ptr, "%s")

#define TUPLE_SPACE_SUPPORTED_PRAGMAS(_)			\
	_(in)						\
	_(out)						\
	_(rd)						\

#define TUPLE_SPACE_PRAGMA_PROCESSOR(pragma)			\
	__tuple_space_process_ ## pragma ## _pragma

#define TUPLE_SPACE_COMMON_F_NAME(t, n)		__attribute__((nonnull)) struct tuple_space_expr_t *__tuple_space_ ## n ## _type_converter(const t arg)
#define TUPLE_SPACE_FORMAL_F_NAME(t, n)		__attribute__((nonnull)) struct tuple_space_expr_t *__tuple_space_ ## n ## _formal_type_converter(t *arg)
#define TUPLE_SPACE_ARR_F_NAME(t, n)		__attribute__((nonnull)) struct tuple_space_expr_t *__tuple_space_ ## n ## _arr_type_converter(t const* arg, unsigned size)
#define TUPLE_SPACE_FORMAL_ARR_F_NAME(t, n)	__attribute__((nonnull)) struct tuple_space_expr_t *__tuple_space_ ## n ## _formal_arr_type_converter(t *arg, unsigned size)

#define TUPLE_SPACE_PROCESS_TYPES(comm, form, arr, form_arr)	\
	TUPLE_SPACE_SUPPORTED_TYPES(comm)			\
	TUPLE_SPACE_SUPPORTED_TYPES(form)			\
	TUPLE_SPACE_SUPPORTED_TYPES(arr)			\
	TUPLE_SPACE_SUPPORTED_TYPES(form_arr)

struct tuple_space_expr_t;

// mk functions prototypes
#define COMM(t, n, ...) TUPLE_SPACE_COMMON_F_NAME(t, n);
#define FORM(t, n, ...) TUPLE_SPACE_FORMAL_F_NAME(t, n);
#define ARR(t, n, ...) TUPLE_SPACE_ARR_F_NAME(t, n);
#define FORM_ARR(t, n, ...) TUPLE_SPACE_FORMAL_ARR_F_NAME(t, n);
TUPLE_SPACE_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)
#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR

typedef int (*tuple_space_cb_t)(void *arg);
int TUPLE_SPACE_PRAGMA_PROCESSOR(eval)(const char *name, tuple_space_cb_t cb, void *arg);

#define DECL_PRAGMA_PROCESSOR(n) \
	void TUPLE_SPACE_PRAGMA_PROCESSOR(n)(int *ret, unsigned n_args, ...);
TUPLE_SPACE_SUPPORTED_PRAGMAS(DECL_PRAGMA_PROCESSOR)
#undef DECL_PRAGMA_PROCESSOR

#define __prgm(t, ...) ({						\
	int COMBINE(ret_, __LINE__) = false;				\
	_Pragma(STR(tnt t COMBINE(ret_, __LINE__), __VA_ARGS__;))	\
	COMBINE(ret_, __LINE__);					\
})
#define in(...) __prgm(in, __VA_ARGS__)
#define out(...) __prgm(out, __VA_ARGS__)
#define rd(...) __prgm(rd, __VA_ARGS__)
#define eval(name, cb, arg) TUPLE_SPACE_PRAGMA_PROCESSOR(eval)(name, cb, arg)

#define tuple_space_set_configuration(host, port) \
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout)
		__attribute__((nonnull(1)));

#endif // __TUPLE_SPACE_H__
