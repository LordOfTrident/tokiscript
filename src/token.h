#ifndef TOKEN_H_HEADER_GUARD
#define TOKEN_H_HEADER_GUARD

#include <assert.h> /* static_assert */
#include <stdlib.h> /* free */

#include "common.h"

typedef enum {
	TOKEN_TYPE_EOF = 0,

	TOKEN_TYPE_ID,
	TOKEN_TYPE_STR,
	TOKEN_TYPE_FMT,
	TOKEN_TYPE_NUM,
	TOKEN_TYPE_NIL,

	TOKEN_TYPE_TRUE,
	TOKEN_TYPE_FALSE,

	TOKEN_TYPE_LET,
	TOKEN_TYPE_CONST,
	TOKEN_TYPE_IF,
	TOKEN_TYPE_WHILE,
	TOKEN_TYPE_FOR,
	TOKEN_TYPE_END,
	TOKEN_TYPE_ELSE,
	TOKEN_TYPE_ELIF,
	TOKEN_TYPE_DO,
	TOKEN_TYPE_RETURN,
	TOKEN_TYPE_DEFER,
	TOKEN_TYPE_FUN,
	TOKEN_TYPE_BREAK,
	TOKEN_TYPE_CONTINUE,

	TOKEN_TYPE_INC,
	TOKEN_TYPE_DEC,
	TOKEN_TYPE_XINC,
	TOKEN_TYPE_XDEC,

	TOKEN_TYPE_ADD,
	TOKEN_TYPE_SUB,
	TOKEN_TYPE_MUL,
	TOKEN_TYPE_DIV,
	TOKEN_TYPE_POW,
	TOKEN_TYPE_MOD,

	TOKEN_TYPE_ASSIGN,
	TOKEN_TYPE_EQUALS,
	TOKEN_TYPE_NOT_EQUALS,
	TOKEN_TYPE_GREATER,
	TOKEN_TYPE_GREATER_EQU,
	TOKEN_TYPE_LESS,
	TOKEN_TYPE_LESS_EQU,

	TOKEN_TYPE_AND,
	TOKEN_TYPE_OR,
	TOKEN_TYPE_NOT,
	TOKEN_TYPE_IN,

	TOKEN_TYPE_LPAREN,
	TOKEN_TYPE_RPAREN,
	TOKEN_TYPE_LSQUARE,
	TOKEN_TYPE_RSQUARE,
	TOKEN_TYPE_COMMA,
	TOKEN_TYPE_SEMICOLON,

	TOKEN_TYPE_ERR,

	TOKEN_TYPE_COUNT,
} token_type_t;

const char *token_type_to_cstr(token_type_t type);

bool token_type_is_bin_op(   token_type_t type);
bool token_type_is_stmts_end(token_type_t type);

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
