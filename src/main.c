#include "main.h"

#define CHOL_ARGS_IMPLEMENTATION
#include <chol/args.h>

#define CHOL_COLORER_IMPLEMENTATION
#include <chol/colorer.h>

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

	/*int    where;
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
		version();*/

	args_t enva = a;
	const char *arg = args_shift(&a);
	if (arg == NULL)
		arg_fatal("No input file");

	if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0)
		usage();
	else if (strcmp(arg, "-v") == 0 || strcmp(arg, "--version") == 0)
		version();

	char *str = readfile(arg);
	if (str == NULL)
		arg_fatal("Could not open file '%s'", arg);

	stmt_t *program = parse(str, arg);
	free(str);

	env_t e;
	env_init(&e, enva.c, enva.v);
	eval(&e, program);
	env_deinit(&e);

	stmt_free(program);

	/*free(stripped.base);*/
	return EXIT_SUCCESS;
}
