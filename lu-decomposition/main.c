#include "tuple_space.h"

int main() {
	const char *str = "test";
	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);

	double a = 2;
	float test() { return 2.5; }
	tuple_space_out(a + 3, 2, str, "TEST", (float)2.45, a, test());

	return 0;
}
