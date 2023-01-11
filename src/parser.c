#include "parser.h"

void fatal(where_t p_where, const char *p_fmt, ...) {
	char    buf[1024];
	va_list args;

	va_start(args, p_fmt);
	vsnprintf(buf, sizeof(buf), p_fmt, args);
	va_end(args);

	fprintf(stderr, "%s:%i:%i: Error: %s\n", p_where.path, p_where.row, p_where.col, buf);
	exit(EXIT_FAILURE);
}

static void parser_advance(parser_t *p_p) {
	if (p_p->tok.type == TOKEN_TYPE_EOF)
		return;

	p_p->tok = lexer_next(&p_p->l);
	if (p_p->tok.type == TOKEN_TYPE_ERR)
		fatal(p_p->tok.where, "%s", p_p->tok.data);
}

static void parser_skip(parser_t *p_p) {
	token_free(&p_p->tok);
	parser_advance(p_p);
}

static expr_t *parse_expr(parser_t *p_p);

static expr_t *parse_expr_call(parser_t *p_p) {
	expr_t *expr       = expr_new();
	expr->where        = p_p->tok.where;
	expr->type         = EXPR_TYPE_CALL;
	expr->as.call.name = p_p->tok.data;

	parser_advance(p_p);
	if (p_p->tok.type != TOKEN_TYPE_LPAREN)
		fatal(p_p->tok.where, "Expected '(', got '%v'", token_type_to_cstr(p_p->tok.type));

	parser_skip(p_p);
	while (p_p->tok.type != TOKEN_TYPE_RPAREN) {
		assert(expr->as.call.args_count < CALL_ARGS_CAPACITY);

		expr->as.call.args[expr->as.call.args_count ++] = parse_expr(p_p);

		if (p_p->tok.type == TOKEN_TYPE_RPAREN)
			break;
		else if (p_p->tok.type != TOKEN_TYPE_COMMA)
			fatal(p_p->tok.where, "Expected ',', got '%v'");

		parser_skip(p_p);
	}

	parser_skip(p_p);
	return expr;
}

static expr_t *parse_expr_str(parser_t *p_p) {
	expr_t *expr = expr_new();
	expr->where  = p_p->tok.where;
	expr->type   = EXPR_TYPE_STR;
	expr->as.str = p_p->tok.data;

	parser_advance(p_p);
	return expr;
}

static expr_t *parse_expr_num(parser_t *p_p) {
	expr_t *expr = expr_new();
	expr->where  = p_p->tok.where;
	expr->type   = EXPR_TYPE_NUM;
	expr->as.num = atof(p_p->tok.data);

	parser_skip(p_p);
	return expr;
}

static expr_t *parse_expr_bool(parser_t *p_p) {
	expr_t *expr   = expr_new();
	expr->where    = p_p->tok.where;
	expr->type     = EXPR_TYPE_BOOL;
	expr->as.bool_ = strcmp(p_p->tok.data, "true") == 0;

	parser_skip(p_p);
	return expr;
}

static expr_t *parse_expr_factor(parser_t *p_p) {
	switch (p_p->tok.type) {
	case TOKEN_TYPE_FALSE:
	case TOKEN_TYPE_TRUE: return parse_expr_bool(p_p);
	case TOKEN_TYPE_ID:   return parse_expr_call(p_p);
	case TOKEN_TYPE_STR:  return parse_expr_str(p_p);
	case TOKEN_TYPE_DEC:  return parse_expr_num(p_p);
	case TOKEN_TYPE_LPAREN: {
		parser_skip(p_p);
		expr_t *expr = parse_expr(p_p);
		if (p_p->tok.type != TOKEN_TYPE_RPAREN)
			fatal(p_p->tok.where, "Expected matching ')', got '%s'",
			      token_type_to_cstr(p_p->tok.type));

		parser_skip(p_p);
		return expr;
	}

	default: fatal(p_p->tok.where, "Unexpected token '%s'", token_type_to_cstr(p_p->tok.type));
	}

	return NULL;
}

static expr_t *parse_expr_term(parser_t *p_p) {
	expr_t *left = parse_expr_factor(p_p);

	while (p_p->tok.type == TOKEN_TYPE_MUL || p_p->tok.type == TOKEN_TYPE_DIV) {
		token_t tok = p_p->tok;
		parser_skip(p_p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_factor(p_p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static expr_t *parse_expr_arith(parser_t *p_p) {
	expr_t *left = parse_expr_term(p_p);

	while (p_p->tok.type == TOKEN_TYPE_ADD || p_p->tok.type == TOKEN_TYPE_SUB) {
		token_t tok = p_p->tok;
		parser_skip(p_p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_term(p_p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static bool token_type_is_comp(token_type_t p_type) {
	switch (p_type) {
	case TOKEN_TYPE_EQUALS:
	case TOKEN_TYPE_NOT_EQUALS:
	case TOKEN_TYPE_GREATER:
	case TOKEN_TYPE_GREATER_EQU:
	case TOKEN_TYPE_LESS:
	case TOKEN_TYPE_LESS_EQU: return true;

	default: return false;
	}
}

static expr_t *parse_expr_comp(parser_t *p_p) {
	expr_t *left = parse_expr_arith(p_p);

	while (token_type_is_comp(p_p->tok.type)) {
		token_t tok = p_p->tok;
		parser_skip(p_p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_arith(p_p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static expr_t *parse_expr(parser_t *p_p) {
	return parse_expr_comp(p_p);
}

static stmt_t *parse_expr_stmt(parser_t *p_p) {
	stmt_t *stmt  = stmt_new();
	stmt->type    = STMT_TYPE_EXPR;
	stmt->where   = p_p->tok.where;
	stmt->as.expr = parse_expr(p_p);

	return stmt;
}

static stmt_t *parse_stmt(parser_t *p_p) {
	switch (p_p->tok.type) {
	default: return parse_expr_stmt(p_p);
	}

	return NULL;
}

stmt_t *parse(const char *p_path) {
	parser_t p = {0};

	lexer_begin(&p.l, p_path);
	p.tok.type = !TOKEN_TYPE_EOF;
	parser_advance(&p);

	stmt_t *program = NULL, *last = NULL;
	while (p.tok.type != TOKEN_TYPE_EOF) {
		stmt_t *stmt = parse_stmt(&p);
		if (program == NULL) {
			program = stmt;
			last    = stmt;
		} else {
			last->next = stmt;
			last = last->next;
		}
	}

	lexer_end(&p.l);
	return program;
}
