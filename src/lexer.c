#include "lexer.h"

static void lexer_advance(lexer_t *p_l) {
	p_l->ch = fgetc(p_l->file);
	if (p_l->ch == '\n') {
		++ p_l->where.row;
		p_l->where.col = 1;
	} else
		++ p_l->where.col;
}

int lexer_begin(lexer_t *p_l, const char *p_path) {
	memset(p_l, 0, sizeof(*p_l));

	p_l->file = fopen(p_path, "r");
	if (p_l->file == NULL)
		return 1;

	p_l->where.path = p_path;
	p_l->where.row  = 1;
	lexer_advance(p_l);

	return 0;
}

void lexer_end(lexer_t *p_l) {
	fclose(p_l->file);
}

static void lexer_tok_append(lexer_t *p_l, char p_ch) {
	assert(p_l->tok_len < TOK_CAPACITY);

	p_l->tok[p_l->tok_len ++] = p_ch;
	p_l->tok[p_l->tok_len]    = 0;
}

static token_t lex_simple_sym(lexer_t *p_l, token_type_t p_type) {
	where_t start = p_l->where;

	lexer_tok_append(p_l, p_l->ch);
	lexer_advance(p_l);

	return token_new(strcpy_to_heap(p_l->tok), p_type, start);
}

static token_t lex_id(lexer_t *p_l) {
	where_t start = p_l->where;

	for (; isalnum(p_l->ch) || p_l->ch == '_'; lexer_advance(p_l))
		lexer_tok_append(p_l, p_l->ch);

	token_type_t type = TOKEN_TYPE_ID;
	if (strcmp(p_l->tok, "true") == 0)
		type = TOKEN_TYPE_TRUE;
	else if (strcmp(p_l->tok, "false") == 0)
		type = TOKEN_TYPE_FALSE;
	else if (strcmp(p_l->tok, "let") == 0)
		type = TOKEN_TYPE_LET;

	return token_new(strcpy_to_heap(p_l->tok), type, start);
}

static token_t lex_str(lexer_t *p_l) {
	where_t start = p_l->where;

	bool escape = false;
	for (lexer_advance(p_l); p_l->ch != '"' || escape; lexer_advance(p_l)) {
		switch (p_l->ch) {
		case EOF: case '\n': return token_new_err("String exceeds line", p_l->where);

		case '\\':
			if (escape) {
				lexer_tok_append(p_l, p_l->ch);
				escape = false;
			} else
				escape = true;

			break;

		default:
			if (escape) {
				char ch;
				switch (p_l->ch) {
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

				default: return token_new_err("Unexpected escape sequence", p_l->where);
				}

				escape = false;
				lexer_tok_append(p_l, ch);
			} else
				lexer_tok_append(p_l, p_l->ch);
		}
	}

	lexer_advance(p_l);

	return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_STR, start);
}

static token_t lex_dec(lexer_t *p_l) {
	where_t start = p_l->where;

	bool floating_point = false;
	for (; isalnum(p_l->ch) || p_l->ch == '.'; lexer_advance(p_l)) {
		if (p_l->ch == '.') {
			if (floating_point)
				return token_new_err("Unexpected '.' in decimal number", p_l->where);

			floating_point = true;
		} else if (!isdigit(p_l->ch))
			return token_new_err("Unexpected character in decimal number", p_l->where);

		lexer_tok_append(p_l, p_l->ch);
	}

	return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_DEC, start);
}

static token_t lex_greater(lexer_t *p_l) {
	where_t start = p_l->where;

	lexer_tok_append(p_l, p_l->ch);
	lexer_advance(p_l);

	if (p_l->ch == '=') {
		lexer_tok_append(p_l, p_l->ch);
		lexer_advance(p_l);

		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_GREATER_EQU, start);
	} else
		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_GREATER, start);
}

static token_t lex_less(lexer_t *p_l) {
	where_t start = p_l->where;

	lexer_tok_append(p_l, p_l->ch);
	lexer_advance(p_l);

	if (p_l->ch == '=') {
		lexer_tok_append(p_l, p_l->ch);
		lexer_advance(p_l);

		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_LESS_EQU, start);
	} else
		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_LESS, start);
}

static token_t lex_slash(lexer_t *p_l) {
	where_t start = p_l->where;

	lexer_tok_append(p_l, p_l->ch);
	lexer_advance(p_l);

	if (p_l->ch == '=') {
		lexer_tok_append(p_l, p_l->ch);
		lexer_advance(p_l);

		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_NOT_EQUALS, start);
	} else
		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_DIV, start);
}

static token_t lex_equals(lexer_t *p_l) {
	where_t start = p_l->where;

	lexer_tok_append(p_l, p_l->ch);
	lexer_advance(p_l);

	if (p_l->ch == '=') {
		lexer_tok_append(p_l, p_l->ch);
		lexer_advance(p_l);

		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_EQUALS, start);
	} else
		return token_new(strcpy_to_heap(p_l->tok), TOKEN_TYPE_ASSIGN, start);
}

static void lexer_skip_comment(lexer_t *p_l) {
	while (p_l->ch != '\n' && p_l->ch != EOF)
		lexer_advance(p_l);
}

token_t lexer_next(lexer_t *p_l) {
	p_l->tok_len = 0;
	p_l->tok[0]  = 0;

	while (p_l->ch != EOF) {
		switch (p_l->ch) {
		case '(': return lex_simple_sym(p_l, TOKEN_TYPE_LPAREN);
		case ')': return lex_simple_sym(p_l, TOKEN_TYPE_RPAREN);
		case ',': return lex_simple_sym(p_l, TOKEN_TYPE_COMMA);

		case '+': return lex_simple_sym(p_l, TOKEN_TYPE_ADD);
		case '-': return lex_simple_sym(p_l, TOKEN_TYPE_SUB);
		case '*': return lex_simple_sym(p_l, TOKEN_TYPE_MUL);
		case '^': return lex_simple_sym(p_l, TOKEN_TYPE_POW);

		case '>': return lex_greater(p_l);
		case '<': return lex_less(p_l);
		case '/': return lex_slash(p_l);
		case '=': return lex_equals(p_l);

		case '"': return lex_str(p_l);

		case '#': lexer_skip_comment(p_l); break;

		default:
			if (isspace(p_l->ch))
				lexer_advance(p_l);
			else if (isalpha(p_l->ch) || p_l->ch == '_')
				return lex_id(p_l);
			else if (isdigit(p_l->ch))
				return lex_dec(p_l);
			else
				return token_new_err("Unexpected character", p_l->where);
		}
	}

	return token_new_eof(p_l->where);
}
