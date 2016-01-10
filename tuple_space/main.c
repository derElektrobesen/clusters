#include <stdlib.h>
#include <stdio.h>

#include "tarantool/tarantool.h"
#include "tarantool/tnt_net.h"
#include "tarantool/tnt_opt.h"

#include "msgpuck/msgpuck.h"

int main() {
	const char *uri = "localhost:5000";
	struct tnt_stream * tnt = tnt_net(NULL); // Allocating stream
	printf("Connecting...\n");
	tnt_set(tnt, TNT_OPT_URI, uri); // Setting URI
	tnt_set(tnt, TNT_OPT_SEND_BUF, 0); // Disable buffering for send
	tnt_set(tnt, TNT_OPT_RECV_BUF, 0); // Disable buffering for recv
	if (tnt_connect(tnt) == -1) {
		printf("Can't connect: %s\n", tnt_strerror(tnt));
		return 0;
	}

	printf("Sending ping...\n");
	tnt_ping(tnt); // Send ping request
	struct tnt_reply * reply = tnt_reply_init(NULL); // Initialize reply
	tnt->read_reply(tnt, reply); // Read reply from server

	printf("Trying to call a function...\n");

	struct tnt_request *req = tnt_request_call(NULL);
	tnt_request_set_funcz(req, "test_func");
	if (tnt_request_set_tuple_format(req, "[[%d,%d,%d]]", 2, 3, 4) == -1) {
		printf("Can't create tuple");
		return -1;
	}

	tnt_request_compile(tnt, req);

	printf("Trying to read reply...\n");
	if (tnt->read_reply(tnt, reply) == -1 || !reply->data) {
		printf("Can't get reply: %s\n", tnt_strerror(tnt));
	} else {
		const char *ptr = reply->data;
		uint32_t size = mp_decode_array(&ptr);
		size = mp_decode_array(&ptr);
		int i = 0;
		printf("Size is %u...\n", size);
		for (; i < size; ++i) {
			uint64_t val = mp_decode_uint(&ptr);
			printf("FOUND: %llu\n", val);
		}

		printf("DONE!\n");
	}

	tnt_request_free(req);

	tnt_reply_free(reply); // Free reply
	tnt_close(tnt); tnt_stream_free(tnt); // Close connection and free stream object

	return 0;
}
