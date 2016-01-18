#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <assert.h>
#include <string.h>

#include "tuple_space_helpers.h"
#include "utils.h"

#define TUPLE_SPACE_SUPPORTED_TYPES(_, G)							\
	_(G, STR, char *)									\
	_(G, INT, int8_t, uint8_t, int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t)	\
	_(G, FLOAT, float)									\
	_(G, DOUBLE, double)

#ifndef __CONCAT
#define __CONCAT(x,y) x ## y
#endif

#define __TUPLE_SPACE_VALUE_TYPE(_type) TUPLE_SPACE_ELEM_TYPE_## _type
#define __TYPENAME(_typename, _index) _typename##_##_index
#define __CONCAT_EX(x, y) __CONCAT(x, y)
#define __VALUE_VAR(_typename, _index) __CONCAT_EX(__TUPLE_SPACE_VALUE_VARIABLE_, __TYPENAME(_typename, _index))
#define __REF_VAR(_typename, _index) __CONCAT_EX(__TUPLE_SPACE_REF_VARIABLE_, __TYPENAME(_typename, _index))

#define __TUPLE_SPACE_TYPES_GENERATOR_EX(GENERATOR, _typename, ...) \
	FOR_EACH_UNDER_RECURSION(GENERATOR, _typename, ##__VA_ARGS__)

// Call this macro to generate a list of variables
// GENERATOR is a macro that takes 3 args: _type (char *), _index (var index) and _typename
#define __TUPLE_SPACE_TYPES_GENERATOR(GENERATOR)						\
	TUPLE_SPACE_SUPPORTED_TYPES(__TUPLE_SPACE_TYPES_GENERATOR_EX, GENERATOR)

enum tuple_space_variable_type_t {
	// Generate list of supported types
#define HELPER(_type, _index, _typename) __VALUE_VAR(_typename, _index),
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER

	__TUPLE_SPACE_VALUE_VARIABLE_MAX,

	// Generate list of references on supported types
#define HELPER(_type, _index, _typename) __REF_VAR(_typename, _index),
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER

	__TUPLE_SPACE_TUPLE_VARIABLE_TUPLE,
	__TUPLE_SPACE_TUPLE_VARIABLE_MASK,

	__TUPLE_SPACE_INVALID_TYPE,
	__TUPLE_SPACE_N_TYPES,
};

struct tuple_space_tuple_t {
	union {
#define HELPER(_type, _index, _typename) _type *__TYPENAME(_typename, _index); /* array of type _type */
		__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	};

	size_t n_items;
	enum tuple_space_variable_type_t item_type;
};

struct tuple_space_value_t {
	// Declare number of values supported
	union {
#define HELPER(_type, _index, _typename) _type __TYPENAME(_typename, _index);
		__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	};

	enum tuple_space_variable_type_t value_type;
};

// Struct will contain a pointers on variables to store a tuple in
struct tuple_space_ref_t {
	union {
#define HELPER(_type, _index, _typename) _type *__TYPENAME(_typename, _index);
		__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	};

	enum tuple_space_variable_type_t ref_type;
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

#ifdef DEBUG
extern const char *tuple_space_real_types[];
#endif

#define __TUPLE_SPACE_TYPE_CHECKER(_type, _typename, _res)					\
	__builtin_choose_expr(									\
		__builtin_types_compatible_p(__typeof(__var_ref), _type),			\
		_res,

#define __TUPLE_SPACE_TYPE_FINDER_HELPER(_type, _index, _typename)					\
	__TUPLE_SPACE_TYPE_CHECKER(_type *, _typename##_##_index, __VALUE_VAR(_typename, _index))	\
	__TUPLE_SPACE_TYPE_CHECKER(const _type *, _typename##_##_index, __VALUE_VAR(_typename, _index))	\
	__TUPLE_SPACE_TYPE_CHECKER(_type const *, _typename##_##_index, __VALUE_VAR(_typename, _index))	\
	__TUPLE_SPACE_TYPE_CHECKER(_type **, _typename##_##_index, __REF_VAR(_typename, _index))	\
	__TUPLE_SPACE_TYPE_CHECKER(const _type **, _typename##_##_index, __REF_VAR(_typename, _index))	\
	__TUPLE_SPACE_TYPE_CHECKER(_type const **, _typename##_##_index, __REF_VAR(_typename, _index))

// Close N breckets from previous macro
#define __TUPLE_SPACE_TYPE_FINDER_CLOSER(...) ))))))

// Macro trying to deduct a type of variable given.
// Result is an item of enum tuple_space_variable_type_t
#define __TUPLE_SPACE_GET_TYPE(_var_) ({							\
	__typeof(_var_) *__var_ref = &(_var_);							\
	/* this will generate 1 more bracket (****) */						\
	__TUPLE_SPACE_TYPE_CHECKER(struct tuple_space_tuple_t **, _unused_stuff_, __TUPLE_SPACE_TUPLE_VARIABLE_TUPLE) \
	__TUPLE_SPACE_TYPE_CHECKER(struct tuple_space_mask_t **, _unused_stuff_, __TUPLE_SPACE_TUPLE_VARIABLE_MASK) \
	__TUPLE_SPACE_TYPES_GENERATOR(__TUPLE_SPACE_TYPE_FINDER_HELPER)				\
	__TUPLE_SPACE_INVALID_TYPE  /* Type not found */					\
	__TUPLE_SPACE_TYPES_GENERATOR(__TUPLE_SPACE_TYPE_FINDER_CLOSER)				\
	/* Close brackets, generated into (****) */ ));						\
})

#define TUPLE_SPACE_POINTER_STORAGER(__val, __index, __unused_stuff)				\
	__typeof(__val) __element_##__index = __val;

#define SHOW_DEDUCTED_TYPE(__value_type)							\
	log_t("Variable type was deducted to '%s'", tuple_space_real_types[__value_type]);	\

#define TUPLE_SPACE_TYPE_DEDUCTOR(__, _index, __ref_supported) ({				\
	log_t("Trying to deduct type of arg #%d", _index);					\
	const int __value_type = __TUPLE_SPACE_GET_TYPE(__element_##_index);			\
												\
	SHOW_DEDUCTED_TYPE(__value_type)							\
	__tuple_space_convertors[								\
		(!__ref_supported								\
			&& (__value_type != __TUPLE_SPACE_TUPLE_VARIABLE_TUPLE)			\
			&& (									\
				(__value_type >= __TUPLE_SPACE_VALUE_VARIABLE_MAX)		\
				|| __value_type == __TUPLE_SPACE_TUPLE_VARIABLE_MASK)		\
			)									\
		? __TUPLE_SPACE_INVALID_TYPE							\
		: __value_type ](__tuple_end_ptr - _index, &__element_##_index);		\
});

#define __tuple_space_wrapper(callback, __ref_supported, ...) ({				\
	/* Store elements in local skope */							\
	FOR_EACH(TUPLE_SPACE_POINTER_STORAGER, __, ##__VA_ARGS__)				\
												\
	/* Declare list of elements to store a tuple */						\
	struct tuple_space_elem_t __real_tuple[ARGS_COUNT(__VA_ARGS__)];			\
	memset(__real_tuple, 0, sizeof(*__real_tuple));						\
												\
	struct tuple_space_elem_t *__tuple_end_ptr = __real_tuple + ARGS_COUNT(__VA_ARGS__) - 1;\
	FOR_EACH(TUPLE_SPACE_TYPE_DEDUCTOR, __ref_supported, ##__VA_ARGS__)			\
												\
	/* We should pass first arg to create va_arg */						\
	callback(ARGS_COUNT(__VA_ARGS__), __real_tuple);					\
})

#define TUPLE(n_items, items) \
	__tuple_space_mk_user_tuple((n_items), (items), __TUPLE_SPACE_GET_TYPE(*(items)))

#define ANY __tuple_space_mk_user_mask(TUPLE_SPACE_MASK_TYPE_ANY)

#define tuple_space_out(...) __tuple_space_wrapper(__tuple_space_out, 0, ##__VA_ARGS__)
#define tuple_space_in(...) __tuple_space_wrapper(__tuple_space_in, 1, ##__VA_ARGS__)

struct tuple_space_tuple_t *__tuple_space_mk_user_tuple(size_t n_items, void *items,
		enum tuple_space_variable_type_t item_type) __attribute__((nonnull));
struct tuple_space_mask_t *__tuple_space_mk_user_mask(enum tuple_space_mask_type_t mask_type);

int __tuple_space_out(int n_elems, const struct tuple_space_elem_t *elems);
int __tuple_space_in(int n_elems, struct tuple_space_elem_t *elems);

#define tuple_space_set_configuration(host, port)						\
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout);

#endif // __TUPLE_SPACE_H__
