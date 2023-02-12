#include "error.h"

void from(where_t where) {
	color_bold(stderr);
	fprintf(stderr, "%s:%i:%i: ", where.path, where.row, where.col);
	color_fg(stderr, COLOR_BCYAN);
	fprintf(stderr, "From here");
	color_reset(stderr);
	fprintf(stderr, "\n");
}

void error(where_t where, const char *fmt, ...) {
	char    buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	color_bold(stderr);
	fprintf(stderr, "%s:%i:%i: ", where.path, where.row, where.col);
	color_fg(stderr, COLOR_BRED);
	fprintf(stderr, "Error:");
	color_reset(stderr);
	fprintf(stderr, " %s\n", buf);

	exit(EXIT_FAILURE);
}

void wrong_type(where_t where, value_type_t type, const char *in) {
	error(where, "Wrong type '%s' in %s", value_type_to_cstr(type), in);
}

void wrong_arg_count(where_t where, size_t got, size_t expected) {
	error(where, "Expected '%zu' argument(s), got '%zu'", expected, got);
}

void undefined(where_t where, const char *name) {
	error(where, "Undefined '%s'", name);
}
