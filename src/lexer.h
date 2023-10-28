#ifndef LEXER_H_HEADER_GUARD
#define LEXER_H_HEADER_GUARD

#include <stdio.h>   /* FILE, fopen, fclose, fgetc, EOF */
#include <stdlib.h>  /* malloc, free */
#include <string.h>  /* memset, strcmp */
#include <ctype.h>   /* isspace, isalpha, isalnum, isdigit */
#include <assert.h>  /* assert */
#include <stdbool.h> /* bool, true, false */

#include "token.h"

#define TOK_CAPACITY 1024

typedef struct {
	size_t      it;
	const char *str;
	int         ch;

	char   tok[TOK_CAPACITY];
	size_t tok_len;

	where_t where;
} lexer_t;

void lexer_init(lexer_t *l, const char *str, const char *path);
token_t lexer_next(lexer_t *l);

#endif
