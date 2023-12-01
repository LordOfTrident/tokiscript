#ifndef MAIN_H_HEADER_GUARD
#define MAIN_H_HEADER_GUARD

#include <stdio.h>  /* printf, stderr, fprintf */
#include <stdlib.h> /* exit, EXIT_FAILURE, EXIT_SUCCESS */
#include <stdarg.h> /* va_list, va_start, va_end, vsnprintf */

#include <chol/args.h>
#include <chol/colorer.h>

#include "common.h"
#include "parser.h"
#include "eval.h"

#define APP_NAME "toki"
#define USAGE    "<PATH | OPTIONS> [...]"

#define VERSION_MAJOR 1
#define VERSION_MINOR 3
#define VERSION_PATCH 0

void usage(void);
void version(void);

#endif
