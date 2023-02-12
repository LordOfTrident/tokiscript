#include "parser.h"

void fatal(where_t where, const char *fmt, ...) {
	char    buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	color_bold(stderr);
	fprintf(stderr, "%s:%i:%i: ", where.path, where.row, where.col);
	color_fg(stderr, COLOR_BRED);
	fprintf(stderr, "Error:");
	color_reset(stderr);
	fprintf(stderr, " %s\n", buf);

	exit(EXIT_FAILURE);
}

static void parser_advance(parser_t *p) {
	if (p->tok.type == TOKEN_TYPE_EOF)
		return;

	p->tok = lexer_next(&p->l);
	if (p->tok.type == TOKEN_TYPE_ERR)
		fatal(p->tok.where, "%s", p->tok.data);
}

static void parser_skip(parser_t *p) {
	token_free(&p->tok);
	parser_advance(p);
}

static expr_t *parse_expr(parser_t *p);

static expr_t *parse_expr_call(parser_t *p, token_t tok) {
	expr_t *expr       = expr_new();
	expr->where        = tok.where;
	expr->type         = EXPR_TYPE_CALL;
	expr->as.call.name = tok.data;

	parser_skip(p);
	while (p->tok.type != TOKEN_TYPE_RPAREN) {
		assert(expr->as.call.args_count < CALL_ARGS_CAPACITY);

		expr->as.call.args[expr->as.call.args_count ++] = parse_expr(p);

		if (p->tok.type == TOKEN_TYPE_RPAREN)
			break;
		else if (p->tok.type != TOKEN_TYPE_COMMA)
			fatal(p->tok.where, "Expected ',', got '%s'", token_type_to_cstr(p->tok.type));

		parser_skip(p);
	}

	parser_skip(p);
	return expr;
}

static expr_t *parse_expr_id(parser_t *p) {
	token_t tok = p->tok;

	parser_advance(p);
	if (p->tok.type == TOKEN_TYPE_LPAREN)
		return parse_expr_call(p, tok);

	expr_t *expr     = expr_new();
	expr->where      = tok.where;
	expr->type       = EXPR_TYPE_ID;
	expr->as.id.name = tok.data;
	return expr;
}

static expr_t *new_value_expr(where_t where) {
	expr_t *expr = expr_new();
	expr->where  = where;
	expr->type   = EXPR_TYPE_VALUE;
	return expr;
}

static expr_t *parse_expr_str(parser_t *p) {
	expr_t *expr = new_value_expr(p->tok.where);
	expr->as.val.type   = VALUE_TYPE_STR;
	expr->as.val.as.str = p->tok.data;

	parser_advance(p);
	return expr;
}

static expr_t *parse_expr_num(parser_t *p) {
	expr_t *expr = new_value_expr(p->tok.where);
	expr->as.val.type   = VALUE_TYPE_NUM;
	expr->as.val.as.num = atof(p->tok.data);

	parser_skip(p);
	return expr;
}

static expr_t *parse_expr_bool(parser_t *p) {
	expr_t *expr = new_value_expr(p->tok.where);
	expr->as.val.type     = VALUE_TYPE_BOOL;
	expr->as.val.as.bool_ = strcmp(p->tok.data, "true") == 0;

	parser_skip(p);
	return expr;
}

static expr_t *parse_expr_factor(parser_t *p) {
	switch (p->tok.type) {
	case TOKEN_TYPE_FALSE:
	case TOKEN_TYPE_TRUE: return parse_expr_bool(p);
	case TOKEN_TYPE_ID:   return parse_expr_id(p);
	case TOKEN_TYPE_STR:  return parse_expr_str(p);
	case TOKEN_TYPE_DEC:  return parse_expr_num(p);
	case TOKEN_TYPE_LPAREN: {
		parser_skip(p);
		expr_t *expr = parse_expr(p);
		if (p->tok.type != TOKEN_TYPE_RPAREN)
			fatal(p->tok.where, "Expected matching ')', got '%s'",
			      token_type_to_cstr(p->tok.type));

		parser_skip(p);
		return expr;
	}

	default: fatal(p->tok.where, "Unexpected token '%s'", token_type_to_cstr(p->tok.type));
	}

	return NULL;
}

static expr_t *parse_expr_term(parser_t *p) {
	expr_t *left = parse_expr_factor(p);

	while (p->tok.type == TOKEN_TYPE_MUL || p->tok.type == TOKEN_TYPE_DIV) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_factor(p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static expr_t *parse_expr_arith(parser_t *p) {
	expr_t *left = parse_expr_term(p);

	while (p->tok.type == TOKEN_TYPE_ADD || p->tok.type == TOKEN_TYPE_SUB) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_term(p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static bool token_type_is_comp(token_type_t type) {
	switch (type) {
	case TOKEN_TYPE_EQUALS:
	case TOKEN_TYPE_NOT_EQUALS:
	case TOKEN_TYPE_GREATER:
	case TOKEN_TYPE_GREATER_EQU:
	case TOKEN_TYPE_LESS:
	case TOKEN_TYPE_LESS_EQU: return true;

	default: return false;
	}
}

static expr_t *parse_expr_comp(parser_t *p) {
	expr_t *left = parse_expr_arith(p);

	while (token_type_is_comp(p->tok.type)) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_arith(p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static expr_t *parse_expr_assign(parser_t *p) {
	expr_t *left = parse_expr_comp(p);

	while (p->tok.type == TOKEN_TYPE_ASSIGN) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_comp(p);
		node->as.bin_op.type  = tok.type;

		left = node;
	}

	return left;
}

static expr_t *parse_expr(parser_t *p) {
	return parse_expr_assign(p);
}

static stmt_t *parse_stmt_expr(parser_t *p) {
	stmt_t *stmt  = stmt_new();
	stmt->type    = STMT_TYPE_EXPR;
	stmt->where   = p->tok.where;
	stmt->as.expr = parse_expr(p);

	return stmt;
}

static stmt_t *parse_stmt_let(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_LET;
	stmt->where  = p->tok.where;

	parser_skip(p);
	if (p->tok.type != TOKEN_TYPE_ID)
		fatal(p->tok.where, "Expected identifier, got '%s'", token_type_to_cstr(p->tok.type));

	stmt->as.let.name = p->tok.data;

	parser_advance(p);
	if (p->tok.type != TOKEN_TYPE_ASSIGN)
		fatal(p->tok.where, "Expected '=', got '%s'", token_type_to_cstr(p->tok.type));

	parser_skip(p);
	stmt->as.let.val = parse_expr(p);

	return stmt;
}

static stmt_t *parse_stmt(parser_t *p) {
	switch (p->tok.type) {
	case TOKEN_TYPE_LET: return parse_stmt_let(p);

	default: return parse_stmt_expr(p);
	}

	return NULL;
}

stmt_t *parse(const char *path, int *status) {
	parser_t p = {0};

	int ret = lexer_begin(&p.l, path);
	if (status != NULL)
		*status = ret;

	if (ret != 0)
		return NULL;

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
