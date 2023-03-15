/* Note: this build file lacks error checks for stuff like malloc, because it just made the code
   less readable and i didnt find it as useful for the build program */

#include <string.h> /* strcmp, strcpy, strlen */
#include <malloc.h> /* malloc, free */
#include <assert.h> /* assert */

/* Directories */
#define SRC "src"
#define BIN "bin"

/* Binary names */
#define OUT     "app"
#define INSTALL "tokiscript"

#define CARGS "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", \
              "-Wno-deprecated-declarations", "-I./"
#define CLIBS "-lm"

#define INSTALL_DIR "/usr/bin"

#define CHOL_BUILDER_IMPLEMENTATION
#include "chol/builder.h"

char *cc = CC;

void uninstall(void) {
#ifdef BUILD_PLATFORM_WINDOWS
	LOG_FATAL("'uninstall' is not supported on Windows yet");
#else
	if (!fs_exists(INSTALL_DIR"/"INSTALL))
		LOG_FATAL("Failed to uninstall (is it installed?)");

	if (fs_remove_file(INSTALL_DIR"/"INSTALL) != 0)
		LOG_FATAL("Failed to uninstall (could not remove '%s', do you have permissions?)",
		          INSTALL_DIR"/"INSTALL);

	LOG_INFO("Uninstalled from '%s'", INSTALL_DIR"/"INSTALL);
#endif
}

void install(void) {
#ifdef BUILD_PLATFORM_WINDOWS
	LOG_FATAL("'install' is not supported on Windows yet");
#else
	if (!fs_exists(BIN"/"OUT))
		LOG_FATAL("Please compile before installing");

	if (fs_copy_file(BIN"/"OUT, INSTALL_DIR"/"INSTALL) != 0)
		LOG_FATAL("Failed to install (could not copy to '%s', do you have permissions?)",
		          INSTALL_DIR"/"INSTALL);

	LOG_INFO("Installed to '%s'", INSTALL_DIR"/"INSTALL);
#endif
}

int main(int argc, const char **argv) {
	args_t a = build_init(argc, argv);
	/* Add the 'clean' subcommand to the usage */
	build_set_usage("[clean | install | uninstall] [OPTIONS]");

	flag_cstr(NULL, "CC", "The C compiler path", &cc);

	args_t stripped; /* The flagless arguments */
	build_parse_args(&a, &stripped);

	const char *subcmd = args_shift(&a);
	if (subcmd != NULL) {
		if (a.c > 0) {
			build_arg_error("Unexpected argument '%s' for '%s'", a.v[0], subcmd);
			exit(EXIT_FAILURE);
		}

		if (strcmp(subcmd, "clean") == 0) {
			build_clean(BIN);

			if (fs_exists(BIN"/"OUT))
				fs_remove_file(BIN"/"OUT);
		} else if (strcmp(subcmd, "install") == 0)
			install();
		else if (strcmp(subcmd, "uninstall") == 0)
			uninstall();
		else {
			build_arg_error("Unknown subcommand '%s'", subcmd);
			exit(EXIT_FAILURE);
		}
	} else {
		const char *srcs[] = {SRC};
		build_app_config_t config = {
			.src_ext = "c", .header_ext = "h",
			.bin = BIN, .out = BIN"/"OUT,
			.srcs = srcs, .srcs_count = sizeof(srcs) / sizeof(srcs[0]),
		};
		build_app(cc, &config);
	}

	free(stripped.base);
	return EXIT_SUCCESS;
}
