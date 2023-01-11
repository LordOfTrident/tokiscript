#include "token.h"

void token_free(token_t *p_tok) {
	if (p_tok->data != NULL) {
		free(p_tok->data);
		p_tok->data = NULL;
	}
}

static const char *token_type_to_cstr_map[] = {
	[TOKEN_TYPE_EOF] = "end of file",

	[TOKEN_TYPE_ID]  = "identifier",
	[TOKEN_TYPE_STR] = "string",
	[TOKEN_TYPE_DEC] = "decimal number",

	[TOKEN_TYPE_TRUE]  = "true",
	[TOKEN_TYPE_FALSE] = "false",
	[TOKEN_TYPE_LET]   = "let",

	[TOKEN_TYPE_ADD] = "+",
	[TOKEN_TYPE_SUB] = "-",
	[TOKEN_TYPE_MUL] = "*",
	[TOKEN_TYPE_DIV] = "/",
	[TOKEN_TYPE_POW] = "^",

	[TOKEN_TYPE_ASSIGN]      = "=",
	[TOKEN_TYPE_EQUALS]      = "==",
	[TOKEN_TYPE_NOT_EQUALS]  = "/=",
	[TOKEN_TYPE_GREATER]     = ">",
	[TOKEN_TYPE_GREATER_EQU] = ">=",
	[TOKEN_TYPE_LESS]        = "<",
	[TOKEN_TYPE_LESS_EQU]    = "<=",

	[TOKEN_TYPE_LPAREN] = "(",
	[TOKEN_TYPE_RPAREN] = ")",
	[TOKEN_TYPE_COMMA]  = ",",

	[TOKEN_TYPE_ERR] = "error",
};

static_assert(TOKEN_TYPE_COUNT == 23); /* Add the new token type to the map */

const char *token_type_to_cstr(token_type_t p_type) {
	if (p_type >= TOKEN_TYPE_COUNT)
		UNREACHABLE("Invalid token type");

	return token_type_to_cstr_map[p_type];
}

token_t token_new(char *p_data, token_type_t p_type, where_t p_where) {
	return (token_t){
		.data  = p_data,
		.type  = p_type,
		.where = p_where,
	};
}

token_t token_new_eof(where_t p_where) {
	return token_new(NULL, TOKEN_TYPE_EOF, p_where);
}

token_t token_new_err(char *p_msg, where_t p_where) {
	return token_new(p_msg, TOKEN_TYPE_ERR, p_where);
}
