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

static const char *token_type_to_keyword_map[TOKEN_TYPE_COUNT] = {
	[TOKEN_TYPE_NIL]      = "nil",
	[TOKEN_TYPE_TRUE]     = "true",
	[TOKEN_TYPE_FALSE]    = "false",
	[TOKEN_TYPE_LET]      = "let",
	[TOKEN_TYPE_CONST]    = "const",
	[TOKEN_TYPE_IF]       = "if",
	[TOKEN_TYPE_WHILE]    = "while",
	[TOKEN_TYPE_FOR]      = "for",
	[TOKEN_TYPE_ELIF]     = "elif",
	[TOKEN_TYPE_ELSE]     = "else",
	[TOKEN_TYPE_END]      = "end",
	[TOKEN_TYPE_DO]       = "do",
	[TOKEN_TYPE_RETURN]   = "return",
	[TOKEN_TYPE_DEFER]    = "defer",
	[TOKEN_TYPE_FUN]      = "fun",
	[TOKEN_TYPE_AND]      = "and",
	[TOKEN_TYPE_OR]       = "or",
	[TOKEN_TYPE_NOT]      = "not",
	[TOKEN_TYPE_BREAK]    = "break",
	[TOKEN_TYPE_CONTINUE] = "continue",
	[TOKEN_TYPE_IN]       = "in",
};

static token_t lex_id(lexer_t *l) {
	where_t start = l->where;

	for (; isalnum(l->ch) || l->ch == '_'; lexer_advance(l))
		lexer_tok_append(l, l->ch);

	token_type_t type = TOKEN_TYPE_ID;
	for (size_t i = 0; i < TOKEN_TYPE_COUNT; ++ i) {
		if (token_type_to_keyword_map[i] == NULL)
			continue;

		if (strcmp(l->tok, token_type_to_keyword_map[i]) == 0) {
			type = (token_type_t)i;
			break;
		}
	}

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

static token_t lex_num(lexer_t *l) {
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

	return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_NUM, start);
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
	} else if (l->ch == '/') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_XDEC, start);
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

static token_t lex_add(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '+') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_INC, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_ADD, start);
}

static token_t lex_sub(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '-') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_DEC, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_SUB, start);
}

static token_t lex_mul(lexer_t *l) {
	where_t start = l->where;

	lexer_tok_append(l, l->ch);
	lexer_advance(l);

	if (l->ch == '*') {
		lexer_tok_append(l, l->ch);
		lexer_advance(l);

		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_XINC, start);
	} else
		return token_new(strcpy_to_heap(l->tok), TOKEN_TYPE_MUL, start);
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
		case '[': return lex_simple_sym(l, TOKEN_TYPE_LSQUARE);
		case ']': return lex_simple_sym(l, TOKEN_TYPE_RSQUARE);
		case ',': return lex_simple_sym(l, TOKEN_TYPE_COMMA);
		case ';': return lex_simple_sym(l, TOKEN_TYPE_SEMICOLON);

		case '+': return lex_add(l);
		case '-': return lex_sub(l);
		case '*': return lex_mul(l);
		case '^': return lex_simple_sym(l, TOKEN_TYPE_POW);
		case '%': return lex_simple_sym(l, TOKEN_TYPE_MOD);

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
				return lex_num(l);
			else
				return token_new_err("Unexpected character", l->where);
		}
	}

	return token_new_eof(l->where);
}
