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

static const char *tuple_space_types[TUPLE_SPACE_TYPE_MAX] = {
	[TUPLE_SPACE_TYPE_BOOL] = "BOOL",
	[TUPLE_SPACE_TYPE_STR] = "STR",
	[TUPLE_SPACE_TYPE_INT] = "INT",
	[TUPLE_SPACE_TYPE_BIN] = "BINARY",
	[TUPLE_SPACE_TYPE_FLOAT] = "FLOAT",
	[TUPLE_SPACE_TYPE_DOUBLE] = "DOUBLE",
	[TUPLE_SPACE_TYPE_NIL] = "NIL",
};

static int tuple_space_tuple_add_val(struct tnt_stream *tuple, const struct tuple_space_elem_value_t *val) {
	if (val->val_type < 0 || val->val_type >= TUPLE_SPACE_TYPE_MAX) {
		log_e("Unknown value type found!");
		return -1;
	}

	tnt_object_add_array(tuple, 2); // first arg is elemnt type, second is element value
	tnt_object_add_strz(tuple, tuple_space_types[val->val_type]);

	switch (val->val_type) {
		case TUPLE_SPACE_TYPE_BOOL:
			tnt_object_add_bool(tuple, val->_bool);
			break;
		case TUPLE_SPACE_TYPE_STR:
			tnt_object_add_strz(tuple, val->_str);
			break;
		case TUPLE_SPACE_TYPE_INT:
			tnt_object_add_int(tuple, val->_int);
			break;
		case TUPLE_SPACE_TYPE_BIN:
			tnt_object_add_bin(tuple, val->_bin.value, val->_bin.size);
			break;
		case TUPLE_SPACE_TYPE_FLOAT:
			tnt_object_add_float(tuple, val->_float);
			break;
		case TUPLE_SPACE_TYPE_DOUBLE:
			tnt_object_add_double(tuple, val->_double);
			break;
		default:
			log_e("Unknown value type found!");
			return -1;
	};
	return 0;
}

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

static struct tnt_stream *tuple_space_tuple_mk(int n_elems, va_list ap, int write_only) {
	struct tnt_stream *tuple = tnt_object(NULL);
	tnt_object_add_array(tuple, n_elems);

	int i = 0;
	int abort = 0;
	for (; !abort && i < n_elems; ++i) {
		struct tuple_space_elem_t elem = va_arg(ap, struct tuple_space_elem_t);
		switch (elem.elem_type) {
			case TUPLE_SPACE_ELEM_VALUE:
				if (tuple_space_tuple_add_val(tuple, &elem._val) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_ELEM_TYPE:
				if (write_only && elem._type != TUPLE_SPACE_TYPE_NIL) {
					log_e("Only values are expected in write-only tuples (type found)");
					abort = 1;
				} else if (tuple_space_tuple_add_type(tuple, &elem._type) == -1)
					abort = 1;
				break;
			case TUPLE_SPACE_ELEM_MASK:
				if (write_only) {
					log_e("Only values are expected in write-only tuples (mask found)");
					abort = 1;
				} else if (tuple_space_tuple_add_mask(tuple, &elem._mask) == -1)
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

int _tuple_space_out(int n_elems, ...) {
	va_list ap;
	va_start(ap, n_elems);

	log_t("Trying to send %d-elements tuple into tuple space", n_elems);
	struct tnt_stream *tuple = tuple_space_tuple_mk(n_elems, ap, 1);

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
