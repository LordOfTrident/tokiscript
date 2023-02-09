#include "main.h"

#define CARGS_IMPLEMENTATION
#include <cargs/cargs.h>

#define COLORER_IMPLEMENTATION
#include <colorer/colorer.h>

static void arg_fatal(const char *fmt, ...) {
	char    msg[256];
	va_list args;

	va_start(args, fmt);
	vsnprintf(msg, sizeof(msg), fmt, args);
	va_end(args);

	color_fg(stderr, COLOR_BRED);
	color_bold(stderr);
	fprintf(stderr, "Error:");
	color_reset(stderr);
	fprintf(stderr, " %s\n", msg);

	fprintf(stderr, "Try '"APP_NAME" -h'\n");
	exit(EXIT_FAILURE);
}

void usage(void) {
	args_print_usage(stdout, APP_NAME, USAGE);
	exit(EXIT_SUCCESS);
}

void version(void) {
	printf(APP_NAME" v%i.%i.%i\n", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH);
	exit(EXIT_SUCCESS);
}

int main(int argc, const char **argv) {
	color_init();

	args_t a = new_args(argc, argv);
	args_shift(&a);

	bool help = false;
	bool ver  = false;

	flag_bool("h", "help",    "Show the usage",   &help);
	flag_bool("v", "version", "Show the version", &ver);

	int    where;
	args_t stripped;
	int    err = args_parse_flags(&a, &where, &stripped);
	if (err != ARG_OK) {
		switch (err) {
		case ARG_OUT_OF_MEM:    UNREACHABLE("malloc() fail");
		case ARG_UNKNOWN:       arg_fatal("Unknown flag '%s'", a.v[where]);            break;
		case ARG_MISSING_VALUE: arg_fatal("Flag '%s' is a missing value", a.v[where]); break;

		default: arg_fatal("Incorrect type for flag '%s'", a.v[where]);
		}
	}

	if (help)
		usage();
	else if (ver)
		version();

	if (stripped.c < 1)
		arg_fatal("No input file");

	FOREACH_IN_ARGS(stripped, path, {
		int     status;
		stmt_t *program = parse(path, &status);
		if (status != 0)
			arg_fatal("Could not open file '%s'", path);

		eval(program);
		stmt_free(program);
	});

	return EXIT_SUCCESS;
}
