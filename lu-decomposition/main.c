#include "tuple_space.h"

float test() { return 2.5; }

int to_eval(char *x) {
	for (int i = 0; i < 10; ++i) {
		out("test", i, x);
		int dst;
		if (i) {
			in("test", ?dst, x);
			log_e("FROM TUPLE: %d", dst);
		}
	}
	return 0;
}

int main() {
	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	char *str = "test";
	int tuple[] = { 2, 5, 6, 8, 9 };
	char *tuple_2[] = { "TEST 1", "HOHOHO", "LALALA" };
	int tuple_3[5];
	double a = 2;
	int to_store;

	//char test_c = 1;

	//int arr[5] = { 1, 2, 3, 4, 5 };

	eval("test1", (tuple_space_cb_t)&to_eval, str);
	eval(str, (tuple_space_cb_t)&to_eval, str);
	eval("test2", (tuple_space_cb_t)&to_eval, str);
	eval("test3", (tuple_space_cb_t)&to_eval, str);

	int ret = out(1, 2, 3, "test", str);
	in(a, ?to_store, test(), tuple, tuple_2, ?tuple_3, ?tuple_2, str);

	return ret;
}
