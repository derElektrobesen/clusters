#include <string.h>

#include "tuple_space.h"

#include "tarantool/tarantool.h"
#include "tarantool/tnt_net.h"
#include "tarantool/tnt_opt.h"

#include "msgpuck/msgpuck.h"

inline static void __tuple_space_tuple_add_STR(struct tnt_stream *tuple, const char *str) {
	log_d("Trying to add string '%s' into tuple", str);
	tnt_object_add_strz(tuple, str);
}

inline static void __tuple_space_tuple_add_INT(struct tnt_stream *tuple, int64_t val) {
	log_d("Trying to add integer '%" PRId64 "' into tuple", val);
	tnt_object_add_int(tuple, val);
}

inline static void __tuple_space_tuple_add_FLOAT(struct tnt_stream *tuple, float val) {
	log_d("Trying to add float '%f' into tuple", val);
	tnt_object_add_float(tuple, val);
}

inline static void __tuple_space_tuple_add_DOUBLE(struct tnt_stream *tuple, double val) {
	log_d("Trying to add double '%lf' into tuple", val);
	tnt_object_add_double(tuple, val);
}

#define __CONVERTOR(_c_type, _typename, _index)							\
	__CONCAT_EX(__tuple_space_##_c_type##_convertor_, __TYPENAME(_typename, _index))

#define HELPER(_type, _index, _typename)							\
	inline static struct tuple_space_elem_t *__CONVERTOR(value, _typename, _index)		\
			(struct tuple_space_elem_t *dest, void *data) __attribute__((nonnull));	\
	inline static struct tuple_space_elem_t *__CONVERTOR(value, _typename, _index)		\
			(struct tuple_space_elem_t *dest, void *data) {				\
		log_t("Arg value type is '" #_type "'");					\
		dest->elem_type = TUPLE_SPACE_VALUE_TYPE;					\
		dest->val_elem.value_type = __VALUE_VAR(_typename, _index);			\
		dest->val_elem.__TYPENAME(_typename, _index) = *(_type *)data;			\
		return dest;									\
	}											\
	inline static struct tuple_space_elem_t *__CONVERTOR(ref, _typename, _index)		\
			(struct tuple_space_elem_t *dest, void *data) __attribute__((nonnull));	\
	inline static struct tuple_space_elem_t *__CONVERTOR(ref, _typename, _index)		\
			(struct tuple_space_elem_t *dest, void *data) {				\
		log_t("Arg type is a pointer on '" #_type "'");					\
		dest->elem_type = TUPLE_SPACE_REF_TYPE;						\
		dest->ref_elem.ref_type = __REF_VAR(_typename, _index);				\
		dest->ref_elem.__TYPENAME(_typename, _index) = *(_type **)data;			\
		return dest;									\
	}

__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER

// A pointer on variable which contains a pointer on a tuple (which is allocated on heap) should be passed in data
inline static struct tuple_space_elem_t *__tuple_space_value_convertor_TUPLE(struct tuple_space_elem_t *dest, void *data) {
	struct tuple_space_tuple_t *passed_tuple = *(struct tuple_space_tuple_t **)data;

	memcpy(&dest->tuple_elem, passed_tuple, sizeof(dest->tuple_elem));
	dest->elem_type = TUPLE_SPACE_TUPLE_TYPE;

	log_d("Arg is a Tuple of size %zu", dest->tuple_elem.n_items);

	// XXX: Passed tuple should be generated with TUPLE() macro
	log_t("Freeing a generated Tuple...");
	free(passed_tuple);

	return dest;
}

// A pointer on variable which contains a pointer on a mask (which is allocated on heap) should be passed in data
inline static struct tuple_space_elem_t *__tuple_space_value_convertor_MASK(struct tuple_space_elem_t *dest, void *data) {
	struct tuple_space_mask_t *passed_mask = *(struct tuple_space_mask_t **)data;

	memcpy(&dest->mask_elem, passed_mask , sizeof(dest->mask_elem));
	dest->elem_type = TUPLE_SPACE_MASK_TYPE;

	log_d("Arg is a Mask of type %d", dest->mask_elem.mask_type);

	// XXX: Passed mask should be generated with ANY (or same) macro
	log_t("Freeing a generated Mask...");
	free(passed_mask);

	return dest;
}

static struct tuple_space_elem_t *__tuple_space_invalid_type(struct tuple_space_elem_t *dest __attribute__((unused)),
		void *data __attribute__((unused))) {
	assert(!"Trying to send into tuple space a value of unknown type!");
	return NULL;
}

// A pointer on array of items of type item_type should be passed in items
struct tuple_space_tuple_t *__tuple_space_mk_user_tuple(size_t n_items, void *items,
		enum tuple_space_variable_type_t item_type) {
	struct tuple_space_tuple_t *tup = malloc(sizeof(struct tuple_space_tuple_t));
	tup->item_type = item_type;
	tup->n_items = n_items;

	log_t("Tuple contains %zu items", n_items);

	switch (item_type) {
#define HELPER(_type, _index, _typename)							\
		case __VALUE_VAR(_typename, _index):						\
			tup->__TYPENAME(_typename, _index) = (_type *)items;			\
			log_t("Tuple items have a type '" #_type "'");				\
			break;									\
		case __REF_VAR(_typename, _index):						\
			assert("Tuple can't contain references on type '" #_type "'. "		\
					"Only values are supported.");				\
			break;

		__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER
		default:
			log_e("Unsupported tuple element type: %d!", item_type);
			assert(0);
	}

	return tup;
}

struct tuple_space_mask_t *__tuple_space_mk_user_mask(enum tuple_space_mask_type_t mask_type) {
	struct tuple_space_mask_t *mask = malloc(sizeof(struct tuple_space_mask_t));

	mask->mask_type = mask_type;

	return mask;
}

__tuple_space_convertor_t __tuple_space_convertors[__TUPLE_SPACE_N_TYPES] = {
#define HELPER(_type, _index, _typename)							\
	[__VALUE_VAR(_typename, _index)] = __CONVERTOR(value, _typename, _index),

	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER
#define HELPER(_type, _index, _typename)							\
	[__REF_VAR(_typename, _index)] = __CONVERTOR(ref, _typename, _index),

	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER

	[__TUPLE_SPACE_TUPLE_VARIABLE_TUPLE] = __tuple_space_value_convertor_TUPLE,
	[__TUPLE_SPACE_TUPLE_VARIABLE_MASK] = __tuple_space_value_convertor_MASK,
	[__TUPLE_SPACE_INVALID_TYPE] = __tuple_space_invalid_type,
};

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

static const char *tuple_space_types[__TUPLE_SPACE_N_TYPES] = {
#define HELPER(_type, _index, _typename) [__VALUE_VAR(_typename, _index)] = #_typename,
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
#define HELPER(_type, _index, _typename) [__REF_VAR(_typename, _index)] = #_typename,
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	[__TUPLE_SPACE_TUPLE_VARIABLE_TUPLE] = "TUPLE",
	[__TUPLE_SPACE_TUPLE_VARIABLE_MASK] = "MASK",
};

#ifdef DEBUG
const char *tuple_space_real_types[__TUPLE_SPACE_N_TYPES] = {
	// For debug purposes
#define HELPER(_type, _index, _typename) [__VALUE_VAR(_typename, _index)] = #_type,
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	[__TUPLE_SPACE_VALUE_VARIABLE_MAX] = "__invalid_var__",
#define HELPER(_type, _index, _typename) [__REF_VAR(_typename, _index)] = __STRING(_type *),
	__TUPLE_SPACE_TYPES_GENERATOR(HELPER)
#undef HELPER
	[__TUPLE_SPACE_TUPLE_VARIABLE_TUPLE] = "__tuple__",
	[__TUPLE_SPACE_TUPLE_VARIABLE_MASK] = "__mask__",
	[__TUPLE_SPACE_INVALID_TYPE] = "__invalid__",
};
#endif

static int tuple_space_tuple_add_val(struct tnt_stream *tuple, const struct tuple_space_value_t *val) {
	if (val->value_type < 0 || val->value_type >= __TUPLE_SPACE_VALUE_VARIABLE_MAX) {
		log_e("Unknown value type found!");
		return -1;
	}

	log_t("Value element found");

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, tuple_space_types[val->value_type]);

	switch (val->value_type) {
#define HELPER(_type, _index, _typename)							\
		case __VALUE_VAR(_typename, _index):						\
			__tuple_space_tuple_add_##_typename(tuple, val->__TYPENAME(_typename, _index)); \
			break;

		__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER
		default:
			log_e("Unknown value type found!");
			return -1;
	}
	return 0;
}

static int tuple_space_tuple_add_tuple(struct tnt_stream *tuple,
		const struct tuple_space_tuple_t *val, int read_only) {
	if (val->item_type < 0 || val->item_type >= __TUPLE_SPACE_VALUE_VARIABLE_MAX) {
		assert(!"Tuple shouldn't contain non-values variables");
	}

	log_d("Sending tuple of type %s", tuple_space_types[val->item_type]);

	tnt_object_add_array(tuple, 3 + (read_only ? 0 : 1));
	tnt_object_add_strz(tuple, "TUPLE");
	tnt_object_add_strz(tuple, tuple_space_types[val->item_type]);
	tnt_object_add_int(tuple, val->n_items);

	if (!read_only) {
		tnt_object_add_array(tuple, val->n_items);

		int i = 0;
		for (; i < val->n_items; ++i) {
			switch (val->item_type) {
#define HELPER(_type, _index, _typename)							\
				case __VALUE_VAR(_typename, _index):						\
					__tuple_space_tuple_add_##_typename(tuple, val->__TYPENAME(_typename, _index)[i]); \
					break;

				__TUPLE_SPACE_TYPES_GENERATOR(HELPER)

#undef HELPER
				default:
					log_e("Unknown value type found!");
					return -1;
			}
		}
	}

	return 0;
}

static int tuple_space_tuple_add_type(struct tnt_stream *tuple, const struct tuple_space_ref_t *val) {
	if (val->ref_type <= __TUPLE_SPACE_VALUE_VARIABLE_MAX || val->ref_type >= __TUPLE_SPACE_INVALID_TYPE) {
		assert(!"Ref shouldn't contain values variables");
	}

	log_d("Ref element found");

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, "TYPE");
	tnt_object_add_strz(tuple, tuple_space_types[val->ref_type]);

	return 0;
}

static int tuple_space_tuple_add_mask(struct tnt_stream *tuple, const struct tuple_space_mask_t *val) {
	if (val->mask_type < 0 || val->mask_type >= TUPLE_SPACE_MASK_TYPE_MAX)
		assert(!"Invalid mask type passed!");

	static const char *masks_types[TUPLE_SPACE_MASK_TYPE_MAX] = {
		[TUPLE_SPACE_MASK_TYPE_ANY] = "ANY",
	};

	log_t("Masktype found");

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, "MASK");
	tnt_object_add_strz(tuple, masks_types[val->mask_type]);

	return 0;
}

static struct tnt_stream *tuple_space_tuple_mk(int n_items, const struct tuple_space_elem_t *items, int read_only) {
	struct tnt_stream *tuple = tnt_object(NULL);

	tnt_object_add_array(tuple, n_items);

	int abort = 0;
	int i = 0;
	for (; !abort && i < n_items; ++i) {
		const struct tuple_space_elem_t *elem = items + i;
		log_t("Elem type is %d", elem->elem_type);
		switch (elem->elem_type) {
			case TUPLE_SPACE_VALUE_TYPE:
				if (tuple_space_tuple_add_val(tuple, &elem->val_elem) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_REF_TYPE:
				if (tuple_space_tuple_add_type(tuple, &elem->ref_elem) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_MASK_TYPE:
				if (tuple_space_tuple_add_mask(tuple, &elem->mask_elem) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_TUPLE_TYPE:
				if (tuple_space_tuple_add_tuple(tuple, &elem->tuple_elem, read_only) == -1)
					abort = 1;
				break;
			default:
				log_e("Invalid tuple came! Abort sending");
				abort = 1;
				break;
		}
	}

	if (abort) {
		tnt_stream_free(tuple);
		return NULL;
	}

	return tuple;
}

static int tuple_space_tuple_send(const char *func_name, struct tnt_stream *args, uint32_t *sync) {
	void request_cleanup(struct tnt_request **req) {
		if (req && *req) {
			tnt_request_free(*req);
			*req = NULL;
		}
	}

	assert(tuple_space_configuration.tnt);

	struct tnt_request *req __attribute__((cleanup(request_cleanup))) = tnt_request_call(NULL);

	if (!req) {
		log_e("Can't create call request");
		return -1;
	}

	if (tnt_request_set_funcz(req, func_name) == -1) {
		log_e("Can't set function name");
		return -1;
	}

	if (tnt_request_set_tuple(req, args) == -1) {
		log_e("Can't set arguments for call request");
		return -1;
	}

	if (tnt_request_compile(tuple_space_configuration.tnt, req) == -1) {
		log_e("Can't send request into stream: %s", tnt_strerror(tuple_space_configuration.tnt));
		return -1;
	}

	if (sync)
		*sync = req->hdr.sync;

	return 0;
}

static int tuple_space_tuple_recv(int n_items, struct tuple_space_elem_t *items, uint32_t sync) {
	log_d("Trying to receive data from tuple space");
	return 0;
}

int __tuple_space_out(int n_items, const struct tuple_space_elem_t *items) {
	log_d("Trying to send tuple with %d items into tuple space", n_items);
	struct tnt_stream *tuple = tuple_space_tuple_mk(n_items, items, 0);

	int ret = -1;
	if (tuple) {
		ret = tuple_space_tuple_send("tuple_space.add_tuple", tuple, NULL);
		tnt_stream_free(tuple);
	}

	if (ret != -1)
		log_t("Tuple successfully sent into tuple space");

	return ret;
}

int __tuple_space_in(int n_items, struct tuple_space_elem_t *items) {
	log_d("Trying to get tuple with %d items from tuple space", n_items);
	struct tnt_stream *tuple = tuple_space_tuple_mk(n_items, items, 1);

	int ret = -1;
	uint32_t sync;
	if (tuple) {
		ret = tuple_space_tuple_send("tuple_space.get_tuple", tuple, &sync);
		tnt_stream_free(tuple);
	}

	if (ret != -1)
		log_t("Tuple successfully sent into tuple space");
	else
		return -1;

	ret = tuple_space_tuple_recv(n_items, items, sync);

	if (ret != -1)
		log_t("Tuple successfully returned from tuple space");

	return ret;
}
