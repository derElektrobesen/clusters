#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <sys/time.h>
#include <assert.h>

#include "tuple_space_helpers.h"
#include "utils.h"

// Supported types takes <_> (callback) as argument.
// Callback should take following args:
//	Typename
//	Vartype
//	Varname
//	TntConverterSuffix
//	PrintfSpecifier
//
// If variable is an array, define it with ARRAY_OF
// Otherwise, define it with SIMPLE
#define TUPLE_SPACE_SUPPORTED_TYPES(_, SIMPLE, ARRAY_OF)					\
	_(STR, ARRAY_OF(char), _str, strz, "%s")						\
	_(UINT32, SIMPLE(uint32_t), _uint32, int, "%u")						\
	_(INT32, SIMPLE(int32_t), _int, int, "%d")						\
	_(UINT64, SIMPLE(uint64_t), _uint64, int, "%" PRIu64)					\
	_(INT64, SIMPLE(int64_t), _int64, int, "%" PRId64)					\
	_(FLOAT, SIMPLE(float), _float, float, "%f")						\
	_(DOUBLE, SIMPLE(double), _double, double, "%lf")

// Will be used many times
#define __TUPLE_SPACE_VALUE_TYPE(_type) TUPLE_SPACE_ELEM_TYPE_## _type
#define __TUPLE_SPACE_REF_TYPE(_type) TUPLE_SPACE_ELEM_REF_TYPE_## _type

#define __TUPLE_SPACE_ARRAY_OF_DEFAULT(_type) _type *
#define __TUPLE_SPACE_SIMPLE_DEFAULT(_type) _type
#define __TUPLE_SPACE_DUMMY(...)

enum tuple_space_value_type_t {
	// Declare list of types supported (take first arg from supported_types)
#define HELPER(_typename, ...) __TUPLE_SPACE_VALUE_TYPE(_typename),
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER, __TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT)
	HELPER(MAX)		// number of types
#undef HELPER
};

struct tuple_space_tuple_t {
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype _varname;
		// Only one-level tuples are supported
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER, __TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT)
#undef HELPER
	} *values;
	int n_values;
	enum tuple_space_value_type_t value_type;
};

struct tuple_space_value_t {
	// Declare number of values supported
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype _varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER, __TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT)
#undef HELPER
	};

	enum tuple_space_value_type_t value_type;
};

enum tuple_space_ref_type_t {
#define HELPER(_typename, ...) __TUPLE_SPACE_REF_TYPE(_typename),
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER, __TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT)
	HELPER(TUPLE)
	HELPER(MAX)		// number of types
#undef HELPER
};

// Struct will contain a pointers on variables to store a tuple in
struct tuple_space_ref_t {
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype *_varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER, __TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT)
		HELPER(_, struct tuple_space_tuple_t, _tuple) // We should move tuple from tuple space into user space
#undef HELPER
	};

	enum tuple_space_ref_type_t ref_type;
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

// Check a specific variable type
#define __tuple_space_try_type(_typename, _vartype, _varname)					\
	(__builtin_types_compatible_p(__typeof(*__value), _vartype)				\
	 || __builtin_types_compatible_p(__typeof(*__value), const _vartype))

// Check a specific variable ref type
#define __tuple_space_try_ref_type(_typename, _vartype, _varname) ({				\
	int res = __tuple_space_try_type(_typename, _vartype, _varname);			\
	if (!__ref_supported && res) {								\
		assert(!"Reference on variable can't be passed in write-only tuples!");		\
	}											\
	res;											\
})

// Macro is used to show tuple items type check
// TODO: Remove this macro usage in future release !!!
//#define _log_tuple(_typename, fmt, ...) ({ 1; })
#define _log_tuple(_typename, fmt, ...) ({ log_t(fmt " -> " #_typename "", ##__VA_ARGS__); 1; })

// We have found an item's type. Create a structure ti send into tuplei space
#define __tuple_space_value_found(_typename, _vartype, _varname) ({				\
	(struct tuple_space_elem_t){								\
		.elem_type = TUPLE_SPACE_VALUE_TYPE,						\
		.val_elem = (struct tuple_space_value_t) {					\
			.value_type = __TUPLE_SPACE_VALUE_TYPE(_typename),			\
			._varname = *(_vartype *)__val_ptr,					\
		},										\
	};											\
})

#define __tuple_space_arr_found(_typename, _vartype, _varname) ({				\
	(struct tuple_space_elem_t){								\
		.elem_type = TUPLE_SPACE_VALUE_TYPE,						\
		.val_elem = (struct tuple_space_value_t) {					\
			.value_type = __TUPLE_SPACE_VALUE_TYPE(_typename),			\
			._varname = (_vartype *)__val_ptr,					\
		},										\
	};											\
})

// We have found an item's type. It is a reference. Store reference into struct to use it
// when we will have a specific tuple
#define __tuple_space_ref_found(_typename, _vartype, _varname) ({				\
	(struct tuple_space_elem_t){								\
		.elem_type = TUPLE_SPACE_REF_TYPE,						\
		.ref_elem = (struct tuple_space_ref_t) {					\
			.ref_type = __TUPLE_SPACE_REF_TYPE(_typename),				\
			._varname = (_vartype *)__val_ptr,					\
		},										\
	};											\
})

// We should generate functions to convert real value into tuple_space_elem_t
// __ref_supported, __value and __res should be defined in a parent macro !
#define TUPLE_SPACE_TYPE_DEDUCTOR_MAIN(_typename, _vartype, _varname, func, spec, ...)		\
	/* We can't create a closure: else statement is used */					\
	if (__tuple_space_try_type(_typename, _vartype, _varname)) {				\
		_log_tuple(_typename, "----> VALUE FOUND (" spec ")", *(_vartype *)__val_ptr);	\
		__res = __tuple_space_value_found(_typename, _vartype, _varname);		\
	} else if (__tuple_space_try_ref_type(_typename, _vartype *, _varname)) {		\
		_log_tuple(_typename, "----> REF FOUND");					\
		__res = __tuple_space_ref_found(_typename, _vartype, _varname);			\
	} else

#define TUPLE_SPACE_TYPE_DEDUCTOR_MAIN_ARR(_typename, _vartype, _varname, ...)			\
	/* We can't create a closure: else statement is used */					\
	if (__tuple_space_try_type(_typename, _vartype[], _varname)) {				\
		_log_tuple(_typename, "----> ARRAY FOUND");					\
		__res = __tuple_space_arr_found(_typename, _vartype, _varname);			\
	} else if (__tuple_space_try_ref_type(_typename, _vartype **, _varname)) {		\
		_log_tuple(_typename, "----> POINTER ON ARRAY FOUND");				\
		__res = __tuple_space_ref_found(_typename, _vartype *, _varname);		\
	} else

// ARRAY_OF and SIMPLE will be expanded into a pair (CB, vartype)
// and CB will be called to generate type deductor if needed
#define TUPLE_SPACE_TYPE_DEDUCTOR_EX(_typename, _vartype, CB, ...)				\
	TUPLE_SPACE_TYPE_DEDUCTOR_EX_HELPER(_typename, _vartype, CB, ##__VA_ARGS__)
#define TUPLE_SPACE_TYPE_DEDUCTOR_EX_HELPER(_typename, _vartype, CB, ...)			\
	CB(_typename, _vartype, ##__VA_ARGS__)

// Create more conditions for arrays
#define __TUPLE_SPACE_ARRAY_OF_EX(_type) _type, TUPLE_SPACE_TYPE_DEDUCTOR_MAIN_ARR
#define __TUPLE_SPACE_SIMPLE_EX(_type) _type, __TUPLE_SPACE_DUMMY

#define TYPE_DEDUCTOR(_, __index) ({								\
	__typeof(__element_##__index) *__value = &__element_##__index;	/* To check a variable type */ \
	void *__val_ptr = &__element_##__index;		/* To convert var in a specific type */	\
	struct tuple_space_elem_t __res;		/* To store a tuple item */		\
												\
	/* Generate deductor for each type */							\
	/* Generate conditions for array-types */						\
	TUPLE_SPACE_SUPPORTED_TYPES(TUPLE_SPACE_TYPE_DEDUCTOR_EX,				\
			__TUPLE_SPACE_SIMPLE_EX, __TUPLE_SPACE_ARRAY_OF_EX) {			\
		/* Not an array (or array ref). Check common var type */			\
		TUPLE_SPACE_SUPPORTED_TYPES(TUPLE_SPACE_TYPE_DEDUCTOR_MAIN,			\
				__TUPLE_SPACE_SIMPLE_DEFAULT, __TUPLE_SPACE_ARRAY_OF_DEFAULT) { \
			/* This will be called if type wasn't found */				\
			assert(!"Unsupported argument type!");					\
		}										\
	}											\
	__res;											\
}), /* Comma is required ! */

#define TUPLE_SPACE_POINTER_STORAGER(__val, __index)						\
	__typeof(__val) __element_##__index = __val;

// Generate a tuple creator for a specofic action
#define __tuple_space_wrapper(callback, __ref_supported_, ...) ({				\
	/* Store elements in local skope */							\
	FOR_EACH(TUPLE_SPACE_POINTER_STORAGER, ##__VA_ARGS__)					\
												\
	int __ref_supported = __ref_supported_; /* will be used by TUPLE_SPACE_TYPE_DEDUCTOR_MAIN */ \
	/* We should pass first arg to create va_arg */						\
	callback(ARGS_COUNT(__VA_ARGS__), FOR_EACH(TYPE_DEDUCTOR, ##__VA_ARGS__)		\
			/* Add a marker to stop iteration over a tuple */			\
			NULL);	\
})

// Push a tuple into tuple space.
// We shouldn't accept a pointers on variables.
#define tuple_space_out(...) __tuple_space_wrapper(__tuple_space_out, 0, ##__VA_ARGS__)

// Dummy variable is required
int __tuple_space_out(int n_items, ...);

#define tuple_space_set_configuration(host, port) \
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout);

#endif // __TUPLE_SPACE_H__
