#include "tuple_space.h"

float test() { return 2.5; }

int to_eval(void *x) {
	return 0;
}

int main() {
	//tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	char *str = "test";
	int tuple[] = { 2, 5, 6, 8, 9 };
	char *tuple_2[] = { "TEST 1", "HOHOHO", "LALALA" };
	int tuple_3[5];
	double a = 2;
	int to_store;

	//char test_c = 1;

	//int arr[5] = { 1, 2, 3, 4, 5 };

	int ret = out(1, 2, 3, "test", str);
	in(a, ?to_store, test(), tuple, tuple_2, ?tuple_3, ?tuple_2, str);

	eval("test", &to_eval, str);

	return ret;
}
