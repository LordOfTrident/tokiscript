#include "lexer.h"

static void lexer_advance(lexer_t *l) {
	l->ch = fgetc(l->file);
	if (l->ch == '\n') {
		++ l->where.row;
		l->where.col = 1;
	} else
		++ l->where.col;
}

int lexer_begin(lexer_t *l, const char *path) {
	memset(l, 0, sizeof(*l));

	l->file = fopen(path, "r");
	if (l->file == NULL)
		return -1;

	l->where.path = path;
	l->where.row  = 1;
	lexer_advance(l);
	return 0;
}

void lexer_end(lexer_t *l) {
	fclose(l->file);
}

static void lexer_tok_append(lexer_t *l, char ch) {
	assert(l->tok_len < TOK_CAPACITY);

	l->tok[l->tok_len ++] = ch;
	l->tok[l->tok_len]    = 0;
}

static token_t lex_simple_sym(lexer_t *l, token_type_t type) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);
	return token_new(strcpy_to_heap(l->tok), type, start);
}

static token_t lex_id(lexer_t *l) {
	where_t start = l->where;

	for (; isalnum(l->ch) || l->ch == '_'; lexer_advance(l))
		lexer_tok_append(l, l->ch);

	token_type_t type = TOKEN_TYPE_ID;
	if (strcmp(l->tok, "true") == 0)
		type = TOKEN_TYPE_TRUE;
	else if (strcmp(l->tok, "false") == 0)
		type = TOKEN_TYPE_FALSE;
	else if (strcmp(l->tok, "let") == 0)
		type = TOKEN_TYPE_LET;
	else if (strcmp(l->tok, "if") == 0)
		type = TOKEN_TYPE_IF;
	else if (strcmp(l->tok, "elif") == 0)
		type = TOKEN_TYPE_ELIF;
	else if (strcmp(l->tok, "else") == 0)
		type = TOKEN_TYPE_ELSE;
	else if (strcmp(l->tok, "end") == 0)
		type = TOKEN_TYPE_END;

	return token_new(strcpy_to_heap(l->tok), type, start);
}

static token_t lex_str(lexer_t *l) {
	where_t start = l->where;

	bool escape = false;
	for (lexer_advance(l); l->ch != '"' || escape; lexer_advance(l)) {
		switch (l->ch) {
		case EOF: case '\n': return token_new_err("String exceeds line", l->where);

		case '\\':
			if (escape) {
				lexer_tok_append(l, l->ch);
				escape = false;
			} else
				escape = true;

			break;

		default:
			if (escape) {
				char ch;
				switch (l->ch) {
				case '0': ch = '\0'; break;
				case 'a': ch = '\a'; break;
				case 'b': ch = '\b'; break;
				case 'e': ch = 27;   break;
				case 'f': ch = '\f'; break;
				case 'n': ch = '\n'; break;
				case 'r': ch = '\r'; break;
				case 't': ch = '\t'; break;
				case 'v': ch = '\v'; break;
				case '"': ch = '"';  break;

				default: return token_new_err("Unexpected escape sequence", l->where);
				}

				escape = false;
				lexer_tok_append(l, ch);
			} else
				lexer_tok_append(l, l->ch);
		}
	}

	lexer_advance(l);
	return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_STR, start);
}

static token_t lex_dec(lexer_t *l) {
	where_t start = l->where;

	bool floating_point = false;
	for (; isalnum(l->ch) || l->ch == '.'; lexer_advance(l)) {
		if (l->ch == '.') {
			if (floating_point)
				return token_new_err("Unexpected '.' in decimal number", l->where);

			floating_point = true;
		} else if (!isdigit(l->ch))
			return token_new_err("Unexpected character in decimal number", l->where);

		lexer_tok_append(l, l->ch);
	}

	return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_DEC, start);
}

static token_t lex_greater(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '=') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_GREATER_EQU, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_GREATER, start);
}

static token_t lex_less(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '=') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_LESS_EQU, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_LESS, start);
}

static token_t lex_slash(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '=') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_NOT_EQUALS, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_DIV, start);
}

static token_t lex_equals(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '=') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_EQUALS, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_ASSIGN, start);
}

static void lexer_skip_comment(lexer_t *l) {
	while (l->ch != '\n' && l->ch != EOF)
		lexer_advance(l);
}

token_t lexer_next(lexer_t *l) {
	l->tok_len = 0;
	l->tok[0]  = 0;

	while (l->ch != EOF) {
		switch (l->ch) {
		case '(': return lex_simple_sym(l, TOKEN_TYPE_LPAREN);
		case ')': return lex_simple_sym(l, TOKEN_TYPE_RPAREN);
		case ',': return lex_simple_sym(l, TOKEN_TYPE_COMMA);

		case '+': return lex_simple_sym(l, TOKEN_TYPE_ADD);
		case '-': return lex_simple_sym(l, TOKEN_TYPE_SUB);
		case '*': return lex_simple_sym(l, TOKEN_TYPE_MUL);
		case '^': return lex_simple_sym(l, TOKEN_TYPE_POW);

		case '>': return lex_greater(l);
		case '<': return lex_less(l);
		case '/': return lex_slash(l);
		case '=': return lex_equals(l);

		case '"': return lex_str(l);

		case '#': lexer_skip_comment(l); break;

		default:
			if (isspace(l->ch))
				lexer_advance(l);
			else if (isalpha(l->ch) || l->ch == '_')
				return lex_id(l);
			else if (isdigit(l->ch))
				return lex_dec(l);
			else
				return token_new_err("Unexpected character", l->where);
		}
	}

	return token_new_eof(l->where);
}
