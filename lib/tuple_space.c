#include <stdarg.h>

#include "tuple_space.h"

#include "tarantool/tarantool.h"
#include "tarantool/tnt_net.h"
#include "tarantool/tnt_opt.h"

#include "msgpuck/msgpuck.h"

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

static const char *tuple_space_types[__TUPLE_SPACE_VALUE_TYPE(MAX)] = {
	/* Stringify types */
#define HELPER(_typename, ...) [__TUPLE_SPACE_VALUE_TYPE(_typename)] = #_typename,
	TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
};

static int tuple_space_tuple_add_val(struct tnt_stream *tuple, const struct tuple_space_value_t *val) {
	if (val->value_type < 0 || val->value_type >= __TUPLE_SPACE_VALUE_TYPE(MAX)) {
		log_e("Unknown value type found!");
		return -1;
	}

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, tuple_space_types[val->value_type]);

	switch (val->value_type) {
#define HELPER(_typename, _vartype, _varname, tnt_suffix)					\
		case __TUPLE_SPACE_VALUE_TYPE(_typename):					\
			tnt_object_add_##tnt_suffix(tuple, val->_varname);			\
			break;
		TUPLE_SPACE_SUPPORTED_TYPES(HELPER)
#undef HELPER
		default:
			log_e("Unknown value type found!");
			return -1;
	};
	return 0;
}

/*
static int tuple_space_tuple_add_type(struct tnt_stream *tuple, const enum tuple_space_elem_value_type_t *val) {
	if (*val < 0 || *val >= TUPLE_SPACE_TYPE_MAX) {
		log_e("Unknown value type found!");
		return -1;
	}

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, "TYPE");
	tnt_object_add_strz(tuple, tuple_space_types[*val]);

	return 0;
}

static int tuple_space_tuple_add_mask(struct tnt_stream *tuple, const enum tuple_space_elem_mask_type_t *val) {
	if (*val < 0 || *val >= TUPLE_SPACE_MASK_MAX) {
		log_e("Unknown value type found!");
		return -1;
	}

	static const char *masks_types[TUPLE_SPACE_MASK_MAX] = {
		[TUPLE_SPACE_MASK_ANY] = "ANY",
	};

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, "MASK");
	tnt_object_add_strz(tuple, masks_types[*val]);

	return 0;
}
*/

static struct tnt_stream *tuple_space_tuple_mk(va_list ap) {
	struct tnt_stream *tuple = tnt_object(NULL);

	tnt_object_type(tuple, TNT_SBO_PACKED);
	tnt_object_add_array(tuple, 0);

	int abort = 0;
	while (1) {
		const struct tuple_space_elem_t elem = va_arg(ap, struct tuple_space_elem_t);
		switch (elem.elem_type) {
			case TUPLE_SPACE_VALUE_TYPE:
				if (tuple_space_tuple_add_val(tuple, &elem.val_elem) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_REF_TYPE:
				// TODO
				log_t("Reftype found");
				break;
			case TUPLE_SPACE_MASK_TYPE:
				// TODO
				log_t("Masktype found");
				break;
			case TUPLE_SPACE_TUPLE_TYPE:
				// TODO:
				log_t("Tupletype found");
				break;
			case TUPLE_SPACE_MAX_TYPE:
				log_t("Last elem found in tuple");
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

	tnt_object_container_close(tuple);

	return tuple;
}

static int tuple_space_tuple_send(const char *func_name, struct tnt_stream *args) {
	void request_cleanup(struct tnt_request **req) {
		if (req && *req) {
			tnt_request_free(*req);
			*req = NULL;
		}
	}

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

	return 0;
}

int __tuple_space_out(int dummy, ...) {
	va_list ap;
	va_start(ap, dummy);

	log_t("Trying to send tuple into tuple space");
	struct tnt_stream *tuple = tuple_space_tuple_mk(ap);

	int ret = -1;
	if (tuple) {
		ret = tuple_space_tuple_send("tuple_space.add_tuple", tuple);
		tnt_stream_free(tuple);
	}

	if (ret != -1)
		log_t("Tuples successfully sent into tuple space");

	va_end(ap);
	return ret;
}
