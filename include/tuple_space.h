#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <assert.h>

#include "tuple_space_helpers.h"
#include "utils.h"

#define TUPLE_SPACE_SUPPORTED_TYPES_EX(_, ARR)							\
	_(STR, char *, char *)									\
	_(INT, int64_t, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t) \
	_(FLOAT, float, float)									\
	_(DOUBLE, double, double)

#define TUPLE_SPACE_DUMMY(a) a*
#define TUPLE_SPACE_SUPPORTED_TYPES(_) TUPLE_SPACE_SUPPORTED_TYPES_EX(_, TUPLE_SPACE_DUMMY)

#define __TUPLE_SPACE_VALUE_TYPE(_type) TUPLE_SPACE_ELEM_TYPE_## _type

enum tuple_space_value_type_t {
	// Declare list of types supported (take first arg from supported_types)
#define HELPER(_typename, ...) __TUPLE_SPACE_VALUE_TYPE(_typename),
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
	TUPLE_SPACE_N_TYPES
#undef HELPER
};

enum tuple_space_variable_type_t {
#define HELPER(_typename, ...) __TUPLE_SPACE_VALUE_VARIABLE_##_typename,
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	__TUPLE_SPACE_VALUE_VARIABLE_MAX, // needed for unsupported refs finder
#define HELPER(_typename, ...) __TUPLE_SPACE_REF_VARIABLE_##_typename,
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	__TUPLE_SPACE_TUPLE_VARIABLE_TUPLE,
	__TUPLE_SPACE_INVALID_TYPE
};


struct tuple_space_tuple_t {
	union {
#define HELPER(_varname, _vartype, ...) _vartype *_varname; /* array of type _vartype */
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
#define HELPER(_varname, _vartype, ...) _vartype **_varname##_PTR; /* pointer on array of type _vartype */
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	};

	size_t n_items;
	enum tuple_space_variable_type_t item_type;
};

struct tuple_space_value_t {
	// Declare number of values supported
	union {
#define HELPER(_varname, _vartype, ...) _vartype _varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	};

	enum tuple_space_value_type_t value_type;
};

// Struct will contain a pointers on variables to store a tuple in
struct tuple_space_ref_t {
	union {
#define HELPER(_varname, _vartype, ...) _vartype *_varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	};

	enum tuple_space_value_type_t ref_type;
};

enum tuple_space_mask_type_t {
	TUPLE_SPACE_MASK_TYPE_ANY,

	TUPLE_SPACE_MASK_TYPE_MAX
};

struct tuple_space_mask_t {
	enum tuple_space_mask_type_t mask_type;
};

enum tuple_space_elem_type_t {
	// Declare global elements types
	TUPLE_SPACE_VALUE_TYPE,		// Element is a value (string, int, ...)
	TUPLE_SPACE_REF_TYPE,		// Element is a pointer on an existing variable (with known type)
	TUPLE_SPACE_MASK_TYPE,		// Element is a mask (match any element)
	TUPLE_SPACE_TUPLE_TYPE,		// Element is a tuple with elements of one type

	TUPLE_SPACE_MAX_TYPE		// Maximum number of types
};

struct tuple_space_elem_t {
	enum tuple_space_elem_type_t elem_type;

	union {
		struct tuple_space_value_t val_elem;
		struct tuple_space_tuple_t tuple_elem;
		struct tuple_space_mask_t mask_elem;
		struct tuple_space_ref_t ref_elem;
	};
};

typedef struct tuple_space_elem_t *(*__tuple_space_convertor_t)(struct tuple_space_elem_t *dest, void *data);
extern __tuple_space_convertor_t __tuple_space_convertors[];

#define __TUPLE_SPACE_TYPE_CHECKER(_type, _typename, _suffix)					\
	__builtin_choose_expr(									\
		__builtin_types_compatible_p(__typeof(__var_ref), _type),			\
		__TUPLE_SPACE_##_suffix##_VARIABLE_##_typename,

#define __TUPLE_SPACE_TYPE_FINDER_HELPER(_type, _index, _typename)				\
	__TUPLE_SPACE_TYPE_CHECKER(_type *, _typename, VALUE)					\
	__TUPLE_SPACE_TYPE_CHECKER(const _type *, _typename, VALUE)				\
	__TUPLE_SPACE_TYPE_CHECKER(_type const *, _typename, VALUE)				\
	__TUPLE_SPACE_TYPE_CHECKER(_type **, _typename, REF)				\
	__TUPLE_SPACE_TYPE_CHECKER(const _type **, _typename, REF)			\
	__TUPLE_SPACE_TYPE_CHECKER(_type const **, _typename, REF)

// Close N breckets from previous macro
#define __TUPLE_SPACE_TYPE_FINDER_CLOSER_EX(...) ))))))

// Iterate over a list of types in each SUPPORTED_TYPE
#define __TUPLE_SPACE_TYPE_FINDER(_typename, _var_type, ...)					\
	FOR_EACH_UNDER_RECURSION(__TUPLE_SPACE_TYPE_FINDER_HELPER, _typename, ##__VA_ARGS__)
#define __TUPLE_SPACE_TYPE_FINDER_CLOSER(_typename, _var_type, ...)				\
	FOR_EACH_UNDER_RECURSION(__TUPLE_SPACE_TYPE_FINDER_CLOSER_EX, _typename, ##__VA_ARGS__)

// Macro trying to deduct a type of variable given.
// Result is an item of enum tuple_space_variable_type_t
#define __TUPLE_SPACE_GET_TYPE(_var_) ({							\
	__typeof(_var_) *__var_ref = &(_var_);							\
	/* this will generate 1 more bracket (****) */						\
	__TUPLE_SPACE_TYPE_CHECKER(struct tuple_space_tuple_t **, TUPLE, TUPLE)			\
	TUPLE_SPACE_SUPPORTED_TYPES(__TUPLE_SPACE_TYPE_FINDER)					\
	__TUPLE_SPACE_INVALID_TYPE  /* Type not found */					\
	TUPLE_SPACE_SUPPORTED_TYPES(__TUPLE_SPACE_TYPE_FINDER_CLOSER)				\
	/* Close bracket, generated into (****) */ );						\
})

#define TUPLE_SPACE_POINTER_STORAGER(__val, __index, ...)					\
	__typeof(__val) __element_##__index = __val;

#define TUPLE_SPACE_TYPE_DEDUCTOR(__, _index, __ref_supported) ({				\
	log_t("Trying to deduct type of arg #%d", _index);					\
	const int __value_type = __TUPLE_SPACE_GET_TYPE(__element_##_index);			\
	__tuple_space_convertors[								\
		(!__ref_supported								\
			&& (__value_type != __TUPLE_SPACE_TUPLE_VARIABLE_TUPLE)			\
			&& (__value_type >= __TUPLE_SPACE_VALUE_VARIABLE_MAX)			\
			&& (__value_type < __TUPLE_SPACE_INVALID_TYPE))				\
		? __TUPLE_SPACE_INVALID_TYPE							\
		: __value_type ](__tuple_end_ptr - _index, &__element_##_index);		\
});

#define __tuple_space_wrapper(callback, __ref_supported, ...) ({				\
	/* Store elements in local skope */							\
	FOR_EACH(TUPLE_SPACE_POINTER_STORAGER, __, ##__VA_ARGS__)				\
												\
	/* Declare list of elements to store a tuple */						\
	struct tuple_space_elem_t __real_tuple[ARGS_COUNT(__VA_ARGS__)];			\
	struct tuple_space_elem_t *__tuple_end_ptr = __real_tuple + ARGS_COUNT(__VA_ARGS__) - 1;\
	FOR_EACH(TUPLE_SPACE_TYPE_DEDUCTOR, __ref_supported, ##__VA_ARGS__)			\
												\
	/* We should pass first arg to create va_arg */						\
	callback(ARGS_COUNT(__VA_ARGS__), __real_tuple);					\
})

#define TUPLE(n_items, items) \
	__tuple_space_mk_user_tuple((n_items), (items), __TUPLE_SPACE_GET_TYPE(*(items)))

#define tuple_space_out(...) __tuple_space_wrapper(__tuple_space_out, 0, ##__VA_ARGS__)

struct tuple_space_tuple_t *__tuple_space_mk_user_tuple(size_t n_items, void *items,
		enum tuple_space_variable_type_t item_type) __attribute__((nonnull));
int __tuple_space_out(int n_elems, const struct tuple_space_elem_t *elems);

#endif // __TUPLE_SPACE_H__
