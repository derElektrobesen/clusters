#ifndef __TUPLE_SPACE_H__
#define __TUPLE_SPACE_H__

#include <sys/time.h>
#include "utils.h"

typedef char *		tuple_space_str;
typedef int64_t		tuple_space_int;
typedef float		tuple_space_float;
typedef double		tuple_space_double;
typedef char		tuple_space_bool;

typedef struct {
	char *value;
	size_t size;
} tuple_space_bin;

enum tuple_space_elem_type_t {
	TUPLE_SPACE_ELEM_VALUE,
	TUPLE_SPACE_ELEM_TYPE,
	TUPLE_SPACE_ELEM_MASK
};

enum tuple_space_elem_value_type_t {
	TUPLE_SPACE_TYPE_BOOL = 0,
	TUPLE_SPACE_TYPE_STR,
	TUPLE_SPACE_TYPE_INT,
	TUPLE_SPACE_TYPE_BIN,
	TUPLE_SPACE_TYPE_FLOAT,
	TUPLE_SPACE_TYPE_DOUBLE,
	TUPLE_SPACE_TYPE_NIL,

	TUPLE_SPACE_TYPE_MAX
};

enum tuple_space_elem_mask_type_t {
	TUPLE_SPACE_MASK_ANY,

	TUPLE_SPACE_MASK_MAX
};

struct tuple_space_elem_value_t {
	union {
		tuple_space_bool	_bool;
		tuple_space_str		_str;
		tuple_space_int		_int;
		tuple_space_bin		_bin;
		tuple_space_float	_float;
		tuple_space_double	_double;
	};

	enum tuple_space_elem_value_type_t val_type;
};

struct tuple_space_elem_t {
	union {
		struct tuple_space_elem_value_t _val;
		enum tuple_space_elem_value_type_t _type;
		enum tuple_space_elem_mask_type_t _mask;
	};

	enum tuple_space_elem_type_t elem_type;
};

#define VAL_HELPER(t, var, val) \
	(struct tuple_space_elem_t){ ._val = { .var = val, .val_type = TUPLE_SPACE_TYPE_##t, }, .elem_type = TUPLE_SPACE_ELEM_VALUE, }
#define ANY_HELPER(t) \
	(struct tuple_space_elem_t){ ._type = TUPLE_SPACE_TYPE_##t, .elem_type = TUPLE_SPACE_ELEM_TYPE, }
#define MASK_HELPER(t) \
	(struct tuple_space_elem_t){ ._type = TUPLE_SPACE_MASK_##t, .elem_type = TUPLE_SPACE_ELEM_MASK, }

#define BOOL(v)		VAL_HELPER(BOOL, _bool, v)
#define STR(v)		VAL_HELPER(STR, _str, v)
#define INT(v)		VAL_HELPER(INT, _int, v)
#define BIN(v, s)	\
	(struct tuple_space_elem_t){ ._val = { ._bin = { .value = v, .size = s, }, .val_type = TUPLE_SPACE_VALUE_BIN, }, .elem_type = TUPLE_SPACE_ELEM_VALUE, }
#define FLOAT(v)	VAL_HELPER(FLOAT, _float, v)
#define DOUBLE(v)	VAL_HELPER(DOUBLE, _double, v)
#define ANY_STR		ANY_HELPER(STR)
#define ANY_INT		ANY_HELPER(INT)
#define ANY_BIN		ANY_HELPER(BIN)
#define ANY_FLOAT	ANY_HELPER(FLOAT)
#define ANY_DOUBLE	ANY_HELPER(DOUBLE)
#define NIL		ANY_HELPER(NIL)
#define ANY		MASK_HELPER(ANY)

#define tuple_space_set_configuration(host, port) \
	tuple_space_set_configuration_ex(host, port, NULL, NULL)

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout);

#define tuple_space_out(...) \
	_tuple_space_out(sizeof((struct tuple_space_elem_t []){ __VA_ARGS__ }) / sizeof(struct tuple_space_elem_t), ##__VA_ARGS__)

// XXX: Use tuple_space_out() macro instead
int _tuple_space_out(int n_elems, ...);

#endif
