#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <stdbool.h>
#include <utils.h>

#define TNT_SUPPORTED_TYPES(_)				\
	_(char, char, "%c")				\
	_(int, int, "%d")				\
	_(double, double, "%lf")			\
	_(char *, char_ptr, "%s")

#define TNT_SUPPORTED_PRAGMAS(_)			\
	_(in)						\
	_(out)						\
	_(rd)						\
	_(eval)						\

#define TNT_PRAGMA_PROCESSOR(pragma)			\
	__tnt_process_ ## pragma ## _pragma

#define TNT_COMMON_F_NAME(t, n)		struct tnt_expr_t *__tnt_ ## n ## _type_converter(const t arg)
#define TNT_FORMAL_F_NAME(t, n)		struct tnt_expr_t *__tnt_ ## n ## _formal_type_converter(t *arg)
#define TNT_ARR_F_NAME(t, n)		struct tnt_expr_t *__tnt_ ## n ## _arr_type_converter(t const* arg, unsigned size)
#define TNT_FORMAL_ARR_F_NAME(t, n)	struct tnt_expr_t *__tnt_ ## n ## _formal_arr_type_converter(t *arg, unsigned size)

#define TNT_PROCESS_TYPES(comm, form, arr, form_arr)	\
	TNT_SUPPORTED_TYPES(comm)			\
	TNT_SUPPORTED_TYPES(form)			\
	TNT_SUPPORTED_TYPES(arr)			\
	TNT_SUPPORTED_TYPES(form_arr)

struct tnt_expr_t;

// mk functions prototypes
#define COMM(t, n, ...) TNT_COMMON_F_NAME(t, n);
#define FORM(t, n, ...) TNT_FORMAL_F_NAME(t, n);
#define ARR(t, n, ...) TNT_ARR_F_NAME(t, n);
#define FORM_ARR(t, n, ...) TNT_FORMAL_ARR_F_NAME(t, n);
TNT_PROCESS_TYPES(COMM, FORM, ARR, FORM_ARR)
#undef COMM
#undef FORM
#undef ARR
#undef FORM_ARR

#define DECL_PRAGMA_PROCESSOR(n) \
	bool TNT_PRAGMA_PROCESSOR(n)(unsigned n_args, ...);
TNT_SUPPORTED_PRAGMAS(DECL_PRAGMA_PROCESSOR)
#undef DECL_PRAGMA_PROCESSOR

#define STR(...) STR_I(__VA_ARGS__)
#define STR_I(...) #__VA_ARGS__
#define in(...) _Pragma(STR(tnt in __VA_ARGS__;))
#define out(...) _Pragma(STR(tnt out __VA_ARGS__;)) ({})
#define rd(...) _Pragma(STR(tnt rd __VA_ARGS__;))
#define eval(...) _Pragma(STR(tnt eval __VA_ARGS__;))

#endif // __TUPLE_SPACE_H__
