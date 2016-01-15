#include "tuple_space.h"

int main() {
	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);
//
//	tuple_space_out(STR("Test string"), INT(100500), BOOL(1), NIL);
//
	tuple_space_out("test string", 22, 1);

	return 0;
}
