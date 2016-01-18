#include "tuple_space.h"

int main() {
	const char *str = "test";
	//tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	int tuple[] = { 2, 5, 6, 8, 9 };
	double a = 2;
	float test() { return 2.5; }
	tuple_space_out(a + 3, 2, str, (float)2.45, a, test(), TUPLE(sizeof(tuple) / sizeof(*tuple), tuple));


	return 0;
}
