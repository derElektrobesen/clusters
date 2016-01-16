#include "tuple_space.h"

int main() {
	__typeof("TEST STRING") v = "TEST_STRING";
	if (__builtin_types_compatible_p(__typeof(v), char *))
		log_e("POINTER ON CHAR");
	else if (__builtin_types_compatible_p(__typeof(v), const char *))
		log_e("CONST POINTER ON CHAR");
	else if (__builtin_types_compatible_p(__typeof(v), char []))
		log_e("ARRAY OF CHAR");
	else if (__builtin_types_compatible_p(__typeof(v), const char []))
		log_e("CONST ARRAY OF CHAR");
	else
		assert("UNKNOWN TYPE");

//	tuple_space_set_configuration(TUPLE_SPACE_HOST, TUPLE_SPACE_PORT);
//
//	tuple_space_out(STR("Test string"), INT(100500), BOOL(1), NIL);
//
	const char *str = "test";
	tuple_space_out(&str, 2, str, "TEST");

	log_e("TEST, %s", str);

	return 0;
}
