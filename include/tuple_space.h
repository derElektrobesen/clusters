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
#define TUPLE_SPACE_SUPPORTED_TYPES(_)			\
	_(STR, char *, _str, strz)			\
	_(INT, int64_t, _int, int)			\
	_(FLOAT, float, _float, float)			\
	_(DOUBLE, double, _double, double)

// Will be used many times
#define __TUPLE_SPACE_VALUE_TYPE(_type) TUPLE_SPACE_ELEM_TYPE_## _type
#define __TUPLE_SPACE_REF_TYPE(_type) TUPLE_SPACE_ELEM_REF_TYPE_## _type

enum tuple_space_value_type_t {
	// Declare list of types supported (take first arg from supported_types)
#define HELPER(_typename, ...) __TUPLE_SPACE_VALUE_TYPE(_typename),
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
	HELPER(MAX)		// number of types
#undef HELPER
};

struct tuple_space_tuple_t {
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype _varname;
		// Only one-level tuples are supported
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	} *values;
	int n_values;
	enum tuple_space_value_type_t value_type;
};

struct tuple_space_value_t {
	// Declare number of values supported
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype _varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
	};

	enum tuple_space_value_type_t value_type;
};

enum tuple_space_ref_type_t {
#define HELPER(_typename, ...) __TUPLE_SPACE_REF_TYPE(_typename),
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
	HELPER(TUPLE)
	HELPER(MAX)		// number of types
#undef HELPER
};

// Struct will contain a pointers on variables to store a tuple in
struct tuple_space_ref_t {
	union {
#define HELPER(_typename, _vartype, _varname, ...) _vartype *_varname;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
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

// We should generate functions to convert real value into tuple_space_elem_t
// __ref_supported, __value and __res should be defined in a parent macro !
#define TYPE_DEDUCTOR_(_typename, _vartype, _varname, ...) \
	/* We can't create a closure: else statement is used */ \
	if (__builtin_types_compatible_p(__typeof(__value), _vartype)) \
		__res = (struct tuple_space_elem_t){ \
			.elem_type = TUPLE_SPACE_VALUE_TYPE, \
			.val_elem = (struct tuple_space_value_t){ \
				.value_type = __TUPLE_SPACE_VALUE_TYPE(_typename), \
				._varname = *(_vartype *)__val_impl, \
			}, \
		}; \
	else if (__ref_supported \
		&& (__builtin_types_compatible_p(__typeof(__value), _vartype *) \
			|| __builtin_types_compatible_p(__typeof(__value), const _vartype *))) \
		__res = (struct tuple_space_elem_t){ \
			.elem_type = TUPLE_SPACE_REF_TYPE, \
			.ref_elem = (struct tuple_space_ref_t){ \
				.ref_type = __TUPLE_SPACE_REF_TYPE(_typename), \
				._varname = *(_vartype **)__val_impl, \
			}, \
		}; \
	else

#define TYPE_DEDUCTOR(__val) ({ \
	/* else statement will be last */ \
	__typeof(__val) __value = __val; /* will be used by TYPE_DEDUCTOR_ */ \
	__typeof(__val) *__value_ptr = &__value; \
	void *__val_impl = __value_ptr; \
	struct tuple_space_elem_t __res; \
	/* Generate deductor for each type */ \
	TUPLE_SPACE_SUPPORTED_TYPES(TYPE_DEDUCTOR_) \
	assert("Unsupported argument type!"); /* This will be called if type wasn't found */ \
	__res; \
}),

#define __tuple_space_wrapper(callback, __ref_supported_, ...) ({ \
	int __ref_supported = __ref_supported_; /* will be used by TYPE_DEDUCTOR_ */ \
	callback(0, FOR_EACH(TYPE_DEDUCTOR, ##__VA_ARGS__) (struct tuple_space_elem_t){ .elem_type = TUPLE_SPACE_MAX_TYPE, }); \
})

#define tuple_space_out(...) \
	__tuple_space_wrapper(__tuple_space_out, 0, ##__VA_ARGS__)

int __tuple_space_out(int dummy, ...);

#define tuple_space_set_configuration(host, port) \
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout);

#endif // __TUPLE_SPACE_H__
