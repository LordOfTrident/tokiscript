#include "node.h"

expr_t *expr_new(void) {
	expr_t *expr = (expr_t*)malloc(sizeof(*expr));
	if (expr == NULL)
		UNREACHABLE("malloc() fail");

	memset(expr, 0, sizeof(*expr));

	return expr;
}

void expr_free(expr_t *expr) {
	if (expr == NULL)
		return;

	switch (expr->type) {
	case EXPR_TYPE_VALUE: value_free(&expr->as.val); break;
	case EXPR_TYPE_ID:    free(expr->as.id.name);    break;
	case EXPR_TYPE_CALL:
		for (size_t i = 0; i < expr->as.call.args_count; ++ i)
			expr_free(expr->as.call.args[i]);

		expr_free(expr->as.call.expr);
		break;

	case EXPR_TYPE_BIN_OP:
		expr_free(expr->as.bin_op.left);
		expr_free(expr->as.bin_op.right);
		break;

	case EXPR_TYPE_UN_OP:
		expr_free(expr->as.un_op.expr);
		break;

	case EXPR_TYPE_DO:
		stmt_free(expr->as.do_.body);
		break;

	case EXPR_TYPE_IDX:
		expr_free(expr->as.idx.expr);
		expr_free(expr->as.idx.start);
		expr_free(expr->as.idx.end);
		break;

	case EXPR_TYPE_FMT:
		for (size_t i = 0; i < expr->as.fmt.args_count; ++ i)
			expr_free(expr->as.fmt.args[i]);

		free(expr->as.fmt.str);
		break;

	case EXPR_TYPE_ARR:
		for (size_t i = 0; i < expr->as.arr.size; ++ i)
			expr_free(expr->as.arr.buf[i]);

		free(expr->as.arr.buf);
		break;

	case EXPR_TYPE_FUN:
		for (size_t i = 0; i < expr->as.fun.args_count; ++ i)
			free(expr->as.fun.args[i]);

		stmt_free(expr->as.fun.body);
		break;

	case EXPR_TYPE_IF:
		expr_free(expr->as.if_.cond);
		expr_free(expr->as.if_.a);
		expr_free(expr->as.if_.b);
		break;

	default: break;
	}

	static_assert(EXPR_TYPE_COUNT == 11); /* Add code to free new expressions */

	free(expr);
}

stmt_t *stmt_new(void) {
	stmt_t *stmt = (stmt_t*)malloc(sizeof(*stmt));
	if (stmt == NULL)
		UNREACHABLE("malloc() fail");

	memset(stmt, 0, sizeof(*stmt));

	return stmt;
}

void stmt_free(stmt_t *stmt) {
	if (stmt == NULL)
		return;

	switch (stmt->type) {
	case STMT_TYPE_EXPR: expr_free(stmt->as.expr); break;
	case STMT_TYPE_LET:
		free(stmt->as.let.name);
		expr_free(stmt->as.let.val);
		stmt_free(stmt->as.let.next);
		break;

	case STMT_TYPE_IF:
		expr_free(stmt->as.if_.cond);
		stmt_free(stmt->as.if_.body);
		stmt_free(stmt->as.if_.else_);
		stmt_free(stmt->as.if_.next);
		break;

	case STMT_TYPE_WHILE:
		expr_free(stmt->as.while_.cond);
		stmt_free(stmt->as.while_.body);
		break;

	case STMT_TYPE_FOR:
		stmt_free(stmt->as.for_.init);
		stmt_free(stmt->as.for_.step);
		expr_free(stmt->as.for_.cond);
		stmt_free(stmt->as.for_.body);
		break;

	case STMT_TYPE_FOREACH:
		free(stmt->as.foreach.name);
		if (stmt->as.foreach.it != NULL)
			free(stmt->as.foreach.it);
		expr_free(stmt->as.foreach.in);
		stmt_free(stmt->as.foreach.body);
		break;

	case STMT_TYPE_RETURN:
		expr_free(stmt->as.return_.expr);
		break;

	case STMT_TYPE_DEFER:
		stmt_free(stmt->as.defer.stmt);
		break;

	case STMT_TYPE_FUN:
		expr_free(stmt->as.fun.def);
		free(stmt->as.fun.name);
		break;

	case STMT_TYPE_ENUM:
		free(stmt->as.enum_.name);
		stmt_free(stmt->as.enum_.next);
		break;

	case STMT_TYPE_IMPORT:
		free(stmt->as.import.path);
		stmt_free(stmt->as.import.next);
		break;

	default: break;
	}

	if (stmt->next != NULL)
		stmt_free(stmt->next);

	free(stmt);
}
