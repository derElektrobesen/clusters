#include "tuple_space.h"

int main() {
	const char *str = "test";
	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	int tuple[] = { 2, 5, 6, 8, 9 };
	char *tuple_2[] = { "TEST 1", "HOHOHO", "LALALA" };
	int tuple_3[5];
	double a = 2;
	float test() { return 2.5; }
	int to_store;

	char test_c = 1;

	tuple_space_out(test_c, a + 3, 2, str, (float)2.45, a, test(),
			TUPLE(sizeof(tuple) / sizeof(*tuple), tuple), TUPLE(sizeof(tuple_2) / sizeof(*tuple_2), tuple_2));
	tuple_space_in(&to_store, ANY, TUPLE(5, tuple_3));

	int arr[5] = { 1, 2, 3, 4, 5 };
	tuple_space_out(2, 4, TUPLE(5, arr));
	tuple_space_in(&to_store, ANY, TUPLE(5, tuple_3));


	return 0;
}
