#ifndef TOKEN_H_HEADER_GUARD
#define TOKEN_H_HEADER_GUARD

#include <assert.h> /* static_assert */
#include <stdlib.h> /* free */

#include "common.h"

typedef enum {
	TOKEN_TYPE_EOF = 0,

	TOKEN_TYPE_ID,
	TOKEN_TYPE_STR,
	TOKEN_TYPE_DEC,

	TOKEN_TYPE_TRUE,
	TOKEN_TYPE_FALSE,

	TOKEN_TYPE_LET,
	TOKEN_TYPE_IF,
	TOKEN_TYPE_END,
	TOKEN_TYPE_ELSE,
	TOKEN_TYPE_ELIF,

	TOKEN_TYPE_ADD,
	TOKEN_TYPE_SUB,
	TOKEN_TYPE_MUL,
	TOKEN_TYPE_DIV,
	TOKEN_TYPE_POW,

	TOKEN_TYPE_ASSIGN,
	TOKEN_TYPE_EQUALS,
	TOKEN_TYPE_NOT_EQUALS,
	TOKEN_TYPE_GREATER,
	TOKEN_TYPE_GREATER_EQU,
	TOKEN_TYPE_LESS,
	TOKEN_TYPE_LESS_EQU,

	TOKEN_TYPE_LPAREN,
	TOKEN_TYPE_RPAREN,
	TOKEN_TYPE_COMMA,

	TOKEN_TYPE_ERR,

	TOKEN_TYPE_COUNT,
} token_type_t;

const char *token_type_to_cstr(token_type_t type);

bool token_type_is_bin_op(token_type_t type);

typedef struct {
	const char *path;
	int         row, col;
} where_t;

typedef struct {
	char        *data;
	token_type_t type;
	where_t      where;
} token_t;

void token_free(token_t *tok);

token_t token_new(char *data, token_type_t type, where_t where);
token_t token_new_eof(where_t where);
token_t token_new_err(char *msg, where_t where);

#endif
