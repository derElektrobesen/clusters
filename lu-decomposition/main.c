#include "tuple_space.h"

float test() { return 2.5; }

int main() {
	//tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	const char *str = "test";
	int tuple[] = { 2, 5, 6, 8, 9 };
	char *tuple_2[] = { "TEST 1", "HOHOHO", "LALALA" };
	int tuple_3[5];
	double a = 2;
	int to_store;

	//char test_c = 1;

	//int arr[5] = { 1, 2, 3, 4, 5 };

	out(1, 2, 3, "test");
	in(a, ?to_store, test(), tuple, tuple_2, ?tuple_3, str);


	return 0;
}
