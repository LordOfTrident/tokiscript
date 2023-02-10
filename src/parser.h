#ifndef PARSER_H_HEADER_GUARD
#define PARSER_H_HEADER_GUARD

#include <stdarg.h> /* va_list, va_start, va_end, vsnprintf */
#include <stdio.h>  /* stderr, fprintf */
#include <stdlib.h> /* atof */

#include "common.h"
#include "lexer.h"
#include "token.h"
#include "node.h"

void fatal(where_t where, const char *fmt, ...);

typedef struct {
	lexer_t l;
	token_t tok;
} parser_t;

stmt_t *parse(const char *path, int *status);

#endif
