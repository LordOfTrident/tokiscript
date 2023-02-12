/* Note: this build file lacks error checks for stuff like malloc, because it just made the code
   less readable and i didnt find it as useful for the build program */

#include <string.h> /* strcmp, strcpy, strlen */
#include <malloc.h> /* malloc, free */
#include <assert.h> /* assert */

#define CBUILDER_IMPLEMENTATION
#include "cbuilder/cbuilder.h"

/* Directories */
#define SRC "src"
#define BIN "bin"

/* Binary names */
#define OUT     "app"
#define INSTALL "tokiscript"

#define CARGS "-O2", "-std=c11", "-Wall", "-Wextra", "-Werror", "-pedantic", \
              "-Wno-deprecated-declarations", "-I./lib"
#define CLIBS "-lm"

char *cc = CC;

#define INSTALL_DIR "/usr/bin"

void create_dir_if_missing(const char *path) {
	if (!fs_exists(path))
		fs_create_dir(path);
}

char *build_file(build_cache_t *c, const char *src_name, bool rebuild_all) {
	char *out_name = fs_replace_ext(src_name, "o");
	char *out      = FS_JOIN_PATH(BIN, out_name);
	char *src      = FS_JOIN_PATH(SRC, src_name);
	free(out_name);

	int64_t m_now, m_cached = build_cache_get(c, src);
	fs_time(src, &m_now, NULL);
	if (m_cached != m_now || rebuild_all) {
		build_cache_set(c, src, m_now);
		CMD(cc, "-c", src, "-o", out, CARGS); /* Build the file */
	}

	free(src);
	return out;
}

void build(void) {
	create_dir_if_missing(BIN);

	/* List of compiled object files */
	char  *o_files[32];
	size_t o_files_count = 0;

	build_cache_t c;
	build_cache_load(&c);

	bool rebuild_all = false;

	int status;
	FOREACH_IN_DIR(SRC, dir, ent, {
		if (strcmp(fs_ext(ent.name), "h") != 0)
			continue;

		char *src = FS_JOIN_PATH(SRC, ent.name);
		int64_t m_now;
		int64_t m_cached = build_cache_get(&c, src);
		fs_time(src, &m_now, NULL);
		if (m_cached != m_now) {
			rebuild_all = true;
			build_cache_set(&c, src, m_now);
		}

		free(src);
	}, status);

	FOREACH_IN_DIR(SRC, dir, ent, {
		if (strcmp(fs_ext(ent.name), "c") != 0)
			continue;

		assert(o_files_count < sizeof(o_files) / sizeof(o_files[0]));

		char *out = build_file(&c, ent.name, rebuild_all);
		o_files[o_files_count ++] = out;
	}, status);

	if (status != 0)
		LOG_FATAL("Failed to open directory '%s'", SRC);

	if (o_files_count == 0)
		LOG_INFO("Nothing to rebuild");
	else {
		build_cache_save(&c);

		/* For compiling a variable list of files, we use the COMPILE macro */
		COMPILE(cc, o_files, o_files_count, "-o", BIN"/"OUT, CARGS, CLIBS);

		for (size_t i = 0; i < o_files_count; ++ i)
			free(o_files[i]);
	}

	build_cache_free(&c);
}

void clean(void) {
	bool found = false;
	int  status; /* Directory status, 0 if it was opened succesfully */
	FOREACH_IN_DIR(BIN, dir, ent, {
		/* Only clean .o files and 'app' */
		if (strcmp(fs_ext(ent.name), "o") != 0 && strcmp(ent.name, OUT) != 0)
			continue;

		if (!found)
			found = true;

		char *path = FS_JOIN_PATH(dir.path, ent.name);
		fs_remove_file(path);
		free(path);
	}, status);

	if (status != 0)
		LOG_FATAL("Failed to open directory '%s'", SRC);

	build_cache_delete();

	if (!found)
		LOG_INFO("Nothing to clean");
	else
		LOG_INFO("Cleaned '%s'", BIN);
}

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
		if (strcmp(subcmd, "clean") == 0) {
			if (a.c > 0) {
				build_arg_error("Unexpected argument '%s' for 'clean'", a.v[0]);
				exit(EXIT_FAILURE);
			}

			clean();
		} else if (strcmp(subcmd, "install") == 0) {
			if (a.c > 0) {
				build_arg_error("Unexpected argument '%s' for 'install'", a.v[0]);
				exit(EXIT_FAILURE);
			}

			install();
		} else if (strcmp(subcmd, "uninstall") == 0) {
			if (a.c > 0) {
				build_arg_error("Unexpected argument '%s' for 'uninstall'", a.v[0]);
				exit(EXIT_FAILURE);
			}

			uninstall();
		} else {
			build_arg_error("Unknown subcommand '%s'", subcmd);
			exit(EXIT_FAILURE);
		}
	} else
		build();

	free(stripped.base);
	return EXIT_SUCCESS;
}
