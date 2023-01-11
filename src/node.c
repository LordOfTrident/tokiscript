#include "node.h"

expr_t *expr_new(void) {
	expr_t *expr = (expr_t*)malloc(sizeof(*expr));
	if (expr == NULL)
		UNREACHABLE("malloc() fail");

	memset(expr, 0, sizeof(*expr));

	return expr;
}

void expr_free(expr_t *p_expr) {
	if (p_expr == NULL)
		return;

	switch (p_expr->type) {
	case EXPR_TYPE_STR: free(p_expr->as.str); break;
	case EXPR_TYPE_CALL:
		for (size_t i = 0; i < p_expr->as.call.args_count; ++ i)
			expr_free(p_expr->as.call.args[i]);

		free(p_expr->as.call.name);

		break;

	case EXPR_TYPE_BIN_OP:
		expr_free(p_expr->as.bin_op.left);
		expr_free(p_expr->as.bin_op.right);

		break;

	default: break;
	}

	static_assert(EXPR_TYPE_COUNT == 5); /* Add code to free new expressions */

	free(p_expr);
}

stmt_t *stmt_new(void) {
	stmt_t *stmt = (stmt_t*)malloc(sizeof(*stmt));
	if (stmt == NULL)
		UNREACHABLE("malloc() fail");

	memset(stmt, 0, sizeof(*stmt));

	return stmt;
}

void stmt_free(stmt_t *p_stmt) {
	if (p_stmt == NULL)
		return;

	switch (p_stmt->type) {
	case STMT_TYPE_EXPR: expr_free(p_stmt->as.expr);

	default: break;
	}

	if (p_stmt->next != NULL)
		stmt_free(p_stmt->next);

	free(p_stmt);
}
