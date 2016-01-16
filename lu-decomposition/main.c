#include "tuple_space.h"

int main() {
	const char *str = "test";
//	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);
//
//	tuple_space_out(STR("Test string"), INT(100500), BOOL(1), NIL);
//
	double a = 2;
	float test() { return 2.5; }
	tuple_space_out(a + 3, 2, str, "TEST", (float)2.45, a, test());

	log_e("TEST, %s", str);

	return 0;
}
