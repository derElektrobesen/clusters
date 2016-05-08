#include "tuple_space.h"
#include <time.h>

float test() { return 2.5; }

int to_eval(char *x) {
	for (int i = 0; i < 10; ++i) {
		out("test", time(NULL), x);
		int dst;
		in("test", ?dst, x);
		log_e("FROM TUPLE: %d", dst);
	}
	int tuple[3];
	char *strings[3];
	rd(2.0, 2.5, ?tuple, ?strings, "test");
	for (int i = 0; i < 3; ++i)
		log_e("INT: %d", tuple[i]);
	for (int i = 0;i < 3; ++i)
		log_e("STR: %s", strings[i]);
	return 0;
}

int main() {
	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	char *str = "test";
	int tuple[] = { 2, 5, 6, 8, 9 };
	char *tuple_2[] = { "TEST 1", "HOHOHO", "LALALA" };
	double a = 2;

	//char test_c = 1;

	//int arr[5] = { 1, 2, 3, 4, 5 };

	eval("test1", (tuple_space_cb_t)&to_eval, str);
	eval(str, (tuple_space_cb_t)&to_eval, str);
	eval("test2", (tuple_space_cb_t)&to_eval, str);
	eval("test3", (tuple_space_cb_t)&to_eval, str);

	int ret = out(1, 2, 3, "test", str);
	out(a, test(), tuple, tuple_2, str);

	return ret;
}
