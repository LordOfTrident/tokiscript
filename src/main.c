#include <stdio.h>  /* printf, stderr, fprintf */
#include <stdlib.h> /* exit, EXIT_FAILURE, EXIT_SUCCESS */

#include "parser.h"
#include "eval.h"

static void usage(const char *name) {
	printf("Usage: %s FILE [OPTIONS]\n"
	       "Options:\n"
	       "  -h, --help    Show this message\n",
	       name);
}

int main(int p_argc, char **p_argv) {
	const char *name = shift(&p_argc, &p_argv);

	if (p_argc < 1) {
		fprintf(stderr, "Error: No input file\nTry '%s -h'\n", name);
		exit(EXIT_FAILURE);
	}

	const char *path = NULL;
	const char *arg;
	while (p_argc > 0) {
		arg = shift(&p_argc, &p_argv);

		if (arg[0] == '-') {
			if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
				usage(name);
				exit(EXIT_SUCCESS);
			} else {
				fprintf(stderr, "Error: Unknown option '%s'\n", arg);
				exit(EXIT_FAILURE);
			}
		} else {
			if (path != NULL) {
				fprintf(stderr, "Error: Unexpected argument '%s'\n", arg);
				exit(EXIT_FAILURE);
			}

			path = arg;
		}
	}

	stmt_t *program = parse(path);
	eval(program);
	stmt_free(program);

	return EXIT_SUCCESS;
}
