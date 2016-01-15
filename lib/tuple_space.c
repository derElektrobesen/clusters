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
		return TUPLE_SPACE_ERR;
	}

	if (conf->tnt->read_reply(conf->tnt, conf->reply) == -1) {
		log_e("Can't get reply for ping request: %s", tnt_strerror(conf->tnt));
		return TUPLE_SPACE_ERR;
	}

	log_i("Ping done");
	return TUPLE_SPACE_SUCC;
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
		return TUPLE_SPACE_ERR;
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
		return TUPLE_SPACE_ERR;
	}

	log_i("Connected to %s:%u", conf->host, conf->port);
	return tuple_space_ping();
}

int tuple_space_set_configuration_ex(const char *host, uint16_t port,
		struct timeval *conn_timeout, struct timeval *req_timeout) {
	int printed = snprintf(tuple_space_configuration.host, sizeof(tuple_space_configuration.host), "%s", host);
	if (printed >= sizeof(tuple_space_configuration.host)) {
		log_e("Too long hostname given into tuple_space_set_configuration()");
		return TUPLE_SPACE_ERR;
	}

	tuple_space_configuration.port = port;
	return tuple_space_connect(conn_timeout, req_timeout);
}
