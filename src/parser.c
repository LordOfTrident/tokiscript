#include "parser.h"

bin_op_type_t token_type_to_bin_op_type_map[TOKEN_TYPE_COUNT] = {
	[TOKEN_TYPE_ADD] = BIN_OP_ADD,
	[TOKEN_TYPE_SUB] = BIN_OP_SUB,
	[TOKEN_TYPE_MUL] = BIN_OP_MUL,
	[TOKEN_TYPE_DIV] = BIN_OP_DIV,
	[TOKEN_TYPE_POW] = BIN_OP_POW,
	[TOKEN_TYPE_MOD] = BIN_OP_MOD,

	[TOKEN_TYPE_ASSIGN] = BIN_OP_ASSIGN,
	[TOKEN_TYPE_INC]    = BIN_OP_INC,
	[TOKEN_TYPE_DEC]    = BIN_OP_DEC,
	[TOKEN_TYPE_XINC]   = BIN_OP_XINC,
	[TOKEN_TYPE_XDEC]   = BIN_OP_XDEC,

	[TOKEN_TYPE_EQUALS]      = BIN_OP_EQUALS,
	[TOKEN_TYPE_NOT_EQUALS]  = BIN_OP_NOT_EQUALS,
	[TOKEN_TYPE_GREATER]     = BIN_OP_GREATER,
	[TOKEN_TYPE_GREATER_EQU] = BIN_OP_GREATER_EQU,
	[TOKEN_TYPE_LESS]        = BIN_OP_LESS,
	[TOKEN_TYPE_LESS_EQU]    = BIN_OP_LESS_EQU,

	[TOKEN_TYPE_AND] = BIN_OP_AND,
	[TOKEN_TYPE_OR]  = BIN_OP_OR,
	[TOKEN_TYPE_IN]  = BIN_OP_IN,
};

static bin_op_type_t token_type_to_bin_op_type(token_type_t type) {
	if (type >= TOKEN_TYPE_COUNT)
		UNREACHABLE("Invalid token type");

	return token_type_to_bin_op_type_map[type];
}

un_op_type_t token_type_to_un_op_type_map[TOKEN_TYPE_COUNT] = {
	[TOKEN_TYPE_ADD] = UN_OP_POS,
	[TOKEN_TYPE_SUB] = UN_OP_NEG,

	[TOKEN_TYPE_NOT] = UN_OP_NOT,
};

static un_op_type_t token_type_to_un_op_type(token_type_t type) {
	if (type >= TOKEN_TYPE_COUNT)
		UNREACHABLE("Invalid token type");

	return token_type_to_un_op_type_map[type];
}

static void parser_advance(parser_t *p) {
	if (p->tok.type == TOKEN_TYPE_EOF)
		return;

	p->tok = lexer_next(&p->l);
	if (p->tok.type == TOKEN_TYPE_ERR)
		error(p->tok.where, "%s", p->tok.data);
}

static void parser_skip(parser_t *p) {
	token_free(&p->tok);
	parser_advance(p);
}

static expr_t *parse_expr( parser_t *p);
static stmt_t *parse_stmt( parser_t *p);
static stmt_t *parse_stmts(parser_t *p);

static expr_t *parse_expr_id(parser_t *p) {
	token_t tok = p->tok;
	parser_advance(p);

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

static expr_t *parse_expr_fmt(parser_t *p) {
	expr_t *expr = expr_new();
	expr->where  = p->tok.where;
	expr->type   = EXPR_TYPE_FMT;

	expr->as.fmt.str = p->tok.data;
	parser_advance(p);

	if (p->tok.type != TOKEN_TYPE_LPAREN)
		error(p->tok.where, "Expected '(', got '%s'", token_type_to_cstr(p->tok.type));

	parser_skip(p);
	while (p->tok.type != TOKEN_TYPE_RPAREN) {
		assert(expr->as.fmt.args_count < ARGS_CAPACITY);

		expr->as.fmt.args[expr->as.fmt.args_count ++] = parse_expr(p);

		if (p->tok.type == TOKEN_TYPE_RPAREN)
			break;
		else if (p->tok.type != TOKEN_TYPE_COMMA)
			error(p->tok.where, "Expected ',', got '%s'", token_type_to_cstr(p->tok.type));

		parser_skip(p);
	}

	parser_skip(p);
	return expr;
}

static expr_t *parse_expr_num(parser_t *p) {
	expr_t *expr = new_value_expr(p->tok.where);
	expr->as.val.type   = VALUE_TYPE_NUM;
	expr->as.val.as.num = atof(p->tok.data);

	parser_skip(p);
	return expr;
}

static expr_t *parse_expr_nil(parser_t *p) {
	expr_t *expr = new_value_expr(p->tok.where);
	expr->as.val.type = VALUE_TYPE_NIL;

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

static expr_t *parse_expr_do(parser_t *p) {
	expr_t *expr = expr_new();
	expr->where  = p->tok.where;
	expr->type   = EXPR_TYPE_DO;

	parser_skip(p);
	expr->as.do_.body = parse_stmts(p);
	return expr;
}

static expr_t *parse_expr_fun(parser_t *p) {
	expr_t *expr = expr_new();
	expr->where  = p->tok.where;
	expr->type   = EXPR_TYPE_FUN;

	if (p->tok.type == TOKEN_TYPE_FUN)
		parser_skip(p);
	else
		parser_advance(p);

	if (p->tok.type != TOKEN_TYPE_LPAREN)
		error(p->tok.where, "Expected '(', got '%s'", token_type_to_cstr(p->tok.type));

	parser_skip(p);
	while (p->tok.type != TOKEN_TYPE_RPAREN) {
		assert(expr->as.fun.args_count < ARGS_CAPACITY);

		if (p->tok.type != TOKEN_TYPE_ID)
			error(p->tok.where, "Expected argument name, got '%s'",
			      token_type_to_cstr(p->tok.type));

		expr->as.fun.args[expr->as.fun.args_count ++] = p->tok.data;

		parser_advance(p);
		if (p->tok.type == TOKEN_TYPE_RPAREN)
			break;
		else if (p->tok.type != TOKEN_TYPE_COMMA)
			error(p->tok.where, "Expected ',', got '%s'", token_type_to_cstr(p->tok.type));

		parser_skip(p);
	}

	parser_skip(p);
	expr->as.fun.body = parse_stmts(p);
	return expr;
}

static expr_t *parse_expr_factor(parser_t *p);

static expr_t *parse_expr_un_op(parser_t *p) {
	expr_t *node = expr_new();
	node->type          = EXPR_TYPE_UN_OP;
	node->where         = p->tok.where;
	node->as.un_op.type = token_type_to_un_op_type(p->tok.type);

	parser_skip(p);
	node->as.un_op.expr = parse_expr_factor(p);

	return node;
}

static expr_t *parse_expr_factor(parser_t *p) {
	switch (p->tok.type) {
	case TOKEN_TYPE_FALSE:
	case TOKEN_TYPE_TRUE: return parse_expr_bool(p);
	case TOKEN_TYPE_ID:   return parse_expr_id(p);
	case TOKEN_TYPE_STR:  return parse_expr_str(p);
	case TOKEN_TYPE_FMT:  return parse_expr_fmt(p);
	case TOKEN_TYPE_NUM:  return parse_expr_num(p);
	case TOKEN_TYPE_NIL:  return parse_expr_nil(p);
	case TOKEN_TYPE_LPAREN: {
		parser_skip(p);
		expr_t *expr = parse_expr(p);
		if (p->tok.type != TOKEN_TYPE_RPAREN)
			error(p->tok.where, "Expected matching ')', got '%s'", token_type_to_cstr(p->tok.type));

		parser_skip(p);
		return expr;
	}

	case TOKEN_TYPE_DO:  return parse_expr_do(p);
	case TOKEN_TYPE_FUN: return parse_expr_fun(p);

	case TOKEN_TYPE_ADD:
	case TOKEN_TYPE_SUB:
	case TOKEN_TYPE_NOT: return parse_expr_un_op(p);

	default: error(p->tok.where, "Unexpected token '%s'", token_type_to_cstr(p->tok.type));
	}

	return NULL;
}

static expr_t *parse_expr_idx(parser_t *p) {
	expr_t *expr = parse_expr_factor(p);

	if (p->tok.type == TOKEN_TYPE_LSQUARE) {
		expr_t *idx      = expr_new();
		idx->where       = p->tok.where;
		idx->type        = EXPR_TYPE_IDX;
		idx->as.idx.expr = expr;

		parser_skip(p);
		idx->as.idx.start = parse_expr(p);

		if (p->tok.type == TOKEN_TYPE_COMMA) {
			parser_skip(p);
			idx->as.idx.end = parse_expr(p);
		}

		if (p->tok.type != TOKEN_TYPE_RSQUARE)
			error(p->tok.where, "Expected matching ']', got '%s'", token_type_to_cstr(p->tok.type));

		parser_skip(p);
		return idx;
	} else
		return expr;
}

static expr_t *parse_expr_call(parser_t *p) {
	expr_t *expr = parse_expr_idx(p);

	if (p->tok.type == TOKEN_TYPE_LPAREN) {
		expr_t *call       = expr_new();
		call->where        = p->tok.where;
		call->type         = EXPR_TYPE_CALL;
		call->as.call.expr = expr;

		parser_skip(p);
		while (p->tok.type != TOKEN_TYPE_RPAREN) {
			assert(call->as.call.args_count < ARGS_CAPACITY);

			call->as.call.args[call->as.call.args_count ++] = parse_expr(p);

			if (p->tok.type == TOKEN_TYPE_RPAREN)
				break;
			else if (p->tok.type != TOKEN_TYPE_COMMA)
				error(p->tok.where, "Expected ',', got '%s'", token_type_to_cstr(p->tok.type));

			parser_skip(p);
		}

		parser_skip(p);
		return call;
	} else
		return expr;
}

static expr_t *parse_expr_pow(parser_t *p) {
	expr_t *left = parse_expr_call(p);

	while (p->tok.type == TOKEN_TYPE_POW) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_call(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

		left = node;
	}

	return left;
}

static expr_t *parse_expr_term(parser_t *p) {
	expr_t *left = parse_expr_pow(p);

	while (p->tok.type == TOKEN_TYPE_MUL || p->tok.type == TOKEN_TYPE_DIV ||
	       p->tok.type == TOKEN_TYPE_MOD) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_pow(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

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
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

		left = node;
	}

	return left;
}

static expr_t *parse_expr_in(parser_t *p) {
	expr_t *left = parse_expr_arith(p);

	while (p->tok.type == TOKEN_TYPE_IN) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_arith(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

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
	expr_t *left = parse_expr_in(p);

	while (token_type_is_comp(p->tok.type)) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_in(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

		left = node;
	}

	return left;
}

static expr_t *parse_expr_logic(parser_t *p) {
	expr_t *left = parse_expr_comp(p);

	while (p->tok.type == TOKEN_TYPE_AND || p->tok.type == TOKEN_TYPE_OR) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_comp(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

		left = node;
	}

	return left;
}

static expr_t *parse_expr_inc(parser_t *p) {
	expr_t *left = parse_expr_logic(p);

	while (p->tok.type == TOKEN_TYPE_INC || p->tok.type == TOKEN_TYPE_XINC ||
	       p->tok.type == TOKEN_TYPE_DEC || p->tok.type == TOKEN_TYPE_XDEC) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_logic(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

		left = node;
	}

	return left;
}

static expr_t *parse_expr_assign(parser_t *p) {
	expr_t *left = parse_expr_inc(p);

	while (p->tok.type == TOKEN_TYPE_ASSIGN) {
		token_t tok = p->tok;
		parser_skip(p);

		expr_t *node = expr_new();
		node->type  = EXPR_TYPE_BIN_OP;
		node->where = tok.where;

		/*if (token_type_is_bin_op(p->tok.type)) {
			node->as.bin_op.type = token_type_to_bin_op_type(p->tok.type);
			parser_skip(p);
		} else
			node->as.bin_op.type = BIN_OP_ASSIGN;*/

		node->as.bin_op.left  = left;
		node->as.bin_op.right = parse_expr_inc(p);
		node->as.bin_op.type  = token_type_to_bin_op_type(tok.type);

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

	stmt->as.let.const_ = p->tok.type == TOKEN_TYPE_CONST;

	parser_skip(p);
	if (p->tok.type != TOKEN_TYPE_ID)
		error(p->tok.where, "Expected identifier, got '%s'", token_type_to_cstr(p->tok.type));

	stmt->as.let.name = p->tok.data;

	parser_advance(p);
	if (p->tok.type == TOKEN_TYPE_ASSIGN) {
		parser_skip(p);
		stmt->as.let.val = parse_expr(p);
	}

	if (p->tok.type == TOKEN_TYPE_COMMA)
		stmt->as.let.next = parse_stmt_let(p);

	return stmt;
}

static stmt_t *parse_stmts(parser_t *p) {
	where_t start = p->tok.where;

	stmt_t *stmts = NULL, *last = NULL;
	while (!token_type_is_stmts_end(p->tok.type)) {
		if (p->tok.type == TOKEN_TYPE_EOF) {
			from(start);
			error(p->tok.where, "Expected 'end', got '%s'", token_type_to_cstr(p->tok.type));
		}

		stmt_t *stmt = parse_stmt(p);
		if (stmts == NULL) {
			stmts = stmt;
			last  = stmt;
		} else {
			last->next = stmt;
			last = last->next;
		}
	}

	if (p->tok.type == TOKEN_TYPE_END)
		parser_skip(p);

	return stmts;
}

static stmt_t *parse_stmt_if(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_IF;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.if_.cond = parse_expr(p);
	stmt->as.if_.body = parse_stmts(p);

	if (p->tok.type == TOKEN_TYPE_ELIF)
		stmt->as.if_.next = parse_stmt_if(p);
	else if (p->tok.type == TOKEN_TYPE_ELSE) {
		parser_skip(p);
		stmt->as.if_.else_ = parse_stmts(p);
	}

	return stmt;
}

static stmt_t *parse_stmt_while(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_WHILE;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.while_.cond = parse_expr(p);
	stmt->as.while_.body = parse_stmts(p);

	return stmt;
}

static stmt_t *parse_stmt_for(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_FOR;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.for_.init = parse_stmt(p);

	if (p->tok.type != TOKEN_TYPE_SEMICOLON)
		error(p->tok.where, "Expected ';', got '%s'", token_type_to_cstr(p->tok.type));

	parser_skip(p);
	stmt->as.for_.cond = parse_expr(p);

	if (p->tok.type != TOKEN_TYPE_SEMICOLON)
		error(p->tok.where, "Expected ';', got '%s'", token_type_to_cstr(p->tok.type));

	parser_skip(p);
	stmt->as.for_.step = parse_stmt(p);
	stmt->as.for_.body = parse_stmts(p);

	return stmt;
}

static stmt_t *parse_stmt_return(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_RETURN;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.return_.expr = parse_expr(p);

	return stmt;
}

static stmt_t *parse_stmt_defer(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_DEFER;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.defer.stmt = parse_stmt(p);

	return stmt;
}

static stmt_t *parse_stmt_break(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_BREAK;
	stmt->where  = p->tok.where;
	parser_skip(p);

	return stmt;
}

static stmt_t *parse_stmt_continue(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_CONTINUE;
	stmt->where  = p->tok.where;
	parser_skip(p);

	return stmt;
}

static stmt_t *parse_stmt_fun(parser_t *p) {
	stmt_t *stmt = stmt_new();
	stmt->type   = STMT_TYPE_FUN;
	stmt->where  = p->tok.where;

	parser_skip(p);
	stmt->as.fun.name = p->tok.data;
	stmt->as.fun.def  = parse_expr_fun(p);

	return stmt;
}

static stmt_t *parse_stmt(parser_t *p) {
	switch (p->tok.type) {
	case TOKEN_TYPE_LET:
	case TOKEN_TYPE_CONST:    return parse_stmt_let(p);
	case TOKEN_TYPE_IF:       return parse_stmt_if(p);
	case TOKEN_TYPE_WHILE:    return parse_stmt_while(p);
	case TOKEN_TYPE_FOR:      return parse_stmt_for(p);
	case TOKEN_TYPE_RETURN:   return parse_stmt_return(p);
	case TOKEN_TYPE_DEFER:    return parse_stmt_defer(p);
	case TOKEN_TYPE_BREAK:    return parse_stmt_break(p);
	case TOKEN_TYPE_CONTINUE: return parse_stmt_continue(p);
	case TOKEN_TYPE_FUN:      return parse_stmt_fun(p);

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
