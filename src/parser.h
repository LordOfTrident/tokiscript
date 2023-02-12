#ifndef PARSER_H_HEADER_GUARD
#define PARSER_H_HEADER_GUARD

#include <stdlib.h> /* atof */

#include "common.h"
#include "error.h"
#include "lexer.h"
#include "token.h"
#include "node.h"

typedef struct {
	lexer_t l;
	token_t tok;
} parser_t;

stmt_t *parse(const char *path, int *status);

#endif
