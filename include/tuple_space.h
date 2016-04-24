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
	_(eval)						\

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

#define DECL_PRAGMA_PROCESSOR(n) \
	bool TUPLE_SPACE_PRAGMA_PROCESSOR(n)(unsigned n_args, ...);
TUPLE_SPACE_SUPPORTED_PRAGMAS(DECL_PRAGMA_PROCESSOR)
#undef DECL_PRAGMA_PROCESSOR

#define STR(...) STR_I(__VA_ARGS__)
#define STR_I(...) #__VA_ARGS__
#define in(...) _Pragma(STR(tnt in __VA_ARGS__;))
#define out(...) _Pragma(STR(tnt out __VA_ARGS__;))
#define rd(...) _Pragma(STR(tnt rd __VA_ARGS__;))
#define eval(...) _Pragma(STR(tnt eval __VA_ARGS__;))

#define tuple_space_set_configuration(host, port) \
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout)
		__attribute__((nonnull(1)));

#endif // __TUPLE_SPACE_H__
