#include "eval.h"

void env_init(env_t *e) {
	memset(e, 0, sizeof(*e));
}

static void wrong_type(where_t where, value_type_t type, const char *in) {
	fatal(where, "Wrong type '%s' in %s", value_type_to_cstr(type), in);
}

static void wrong_arg_count(where_t where, size_t got, size_t expected) {
	fatal(where, "Expected '%zu' argument(s), got '%zu'", expected, got);
}

static value_t eval_expr(env_t *e, expr_t *expr);

static value_t builtin_print(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	for (size_t i = 0; i < call->args_count; ++ i) {
		if (i > 0)
			putchar(' ');

		value_t value = eval_expr(e, call->args[i]);
		switch (value.type) {
		case VALUE_TYPE_NIL:  printf("(nil)");            break;
		case VALUE_TYPE_STR:  printf("%s", value.as.str); break;
		case VALUE_TYPE_BOOL: printf("%s", value.as.bool_? "true" : "false"); break;
		case VALUE_TYPE_NUM: {
			char buf[64] = {0};
			double_to_str(value.as.num, buf, sizeof(buf));
			printf("%s", buf);
		} break;

		default: UNREACHABLE("Unknown value type");
		}
	}

	return value_nil();
}

static value_t builtin_println(env_t *e, expr_t *expr) {
	builtin_print(e, expr);
	putchar('\n');

	return value_nil();
}

static value_t builtin_len(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	switch (val.type) {
	case VALUE_TYPE_STR: return value_num(strlen(val.as.str));

	default: wrong_type(expr->where, val.type, "'len' function");
	}

	return value_nil();
}

static builtin_t builtins[] = {
	{.name = "println", .func = builtin_println},
	{.name = "print",   .func = builtin_print},
	{.name = "len",     .func = builtin_len},
};

static value_t eval_expr_call(env_t *e, expr_t *expr) {
	for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); ++ i) {
		if (strcmp(builtins[i].name, expr->as.call.name) == 0)
			return builtins[i].func(e, expr);
	}

	fatal(expr->where, "Unknown function '%s'", expr->as.call.name);
	return value_nil();
}

static var_t *env_get_var(env_t *e, char *name) {
	for (size_t i = 0; i < VARS_CAPACITY; ++ i) {
		if (e->vars[i].name == NULL)
			continue;

		if (strcmp(e->vars[i].name, name) == 0)
			return &e->vars[i];
	}

	return NULL;
}

static value_t eval_expr_id(env_t *e, expr_t *expr) {
	var_t *var = env_get_var(e, expr->as.id.name);
	if (var == NULL)
		fatal(expr->where, "Undefined variable '%s'", expr->as.id.name);

	return var->val;
}

static value_t eval_expr_value(env_t *e, expr_t *expr) {
	UNUSED(e);
	return expr->as.val;
}

static value_t eval_expr_bin_op_equals(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, left.type,
		           "right side of '==' operation, expected same as left side");

	value_t result;
	result.type = VALUE_TYPE_BOOL;

	switch (left.type) {
	case VALUE_TYPE_NUM:  result.as.bool_ = left.as.num   == right.as.num;          break;
	case VALUE_TYPE_BOOL: result.as.bool_ = left.as.bool_ == right.as.bool_;        break;
	case VALUE_TYPE_STR:  result.as.bool_ = strcmp(left.as.str, right.as.str) == 0; break;

	default: wrong_type(expr->where, left.type, "left side of '==' operation");
	}

	return result;
}

static value_t eval_expr_bin_op(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	switch (bin_op->type) {
	case TOKEN_TYPE_EQUALS: return eval_expr_bin_op_equals(e, expr);
	case TOKEN_TYPE_NOT_EQUALS: {
		value_t value  = eval_expr_bin_op_equals(e, expr);
		value.as.bool_ = !value.as.bool_;
		return value;
	}

	case TOKEN_TYPE_ASSIGN: {
		if (bin_op->left->type != EXPR_TYPE_ID)
			fatal(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t  *var = env_get_var(e, name);
		if (var == NULL)
			fatal(expr->where, "Undefined variable '%s'", name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "assignment");

		var->val = val;
		return val;
	} break;

	case TOKEN_TYPE_ADD: {
		value_t left  = eval_expr(e, bin_op->left);
		value_t right = eval_expr(e, bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '+' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '+' operation, expectedcted same as left side");

		left.as.num += right.as.num;
		return left;
	}

	case TOKEN_TYPE_SUB: {
		value_t left  = eval_expr(e, bin_op->left);
		value_t right = eval_expr(e, bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '-' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '-' operation, expected same as left side");

		left.as.num -= right.as.num;
		return left;
	}

	case TOKEN_TYPE_MUL: {
		value_t left  = eval_expr(e, bin_op->left);
		value_t right = eval_expr(e, bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '*' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '*' operation, expected same as left side");

		left.as.num *= right.as.num;
		return left;
	}

	case TOKEN_TYPE_DIV: {
		value_t left  = eval_expr(e, bin_op->left);
		value_t right = eval_expr(e, bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '/' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '/' operation, expected same as left side");

		left.as.num /= right.as.num;
		return left;
	}

	case TOKEN_TYPE_POW: {
		value_t left  = eval_expr(e, bin_op->left);
		value_t right = eval_expr(e, bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '^' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '^' operation, expected same as left side");

		left.as.num = pow(left.as.num, right.as.num);
		return left;
	}

	default: UNREACHABLE("Unknown binary operation type");
	}
}

static value_t eval_expr(env_t *e, expr_t *expr) {
	switch (expr->type) {
	case EXPR_TYPE_CALL:   return eval_expr_call(  e, expr);
	case EXPR_TYPE_ID:     return eval_expr_id(    e, expr);
	case EXPR_TYPE_VALUE:  return eval_expr_value( e, expr);
	case EXPR_TYPE_BIN_OP: return eval_expr_bin_op(e, expr);

	default: UNREACHABLE("Unknown expression type");
	}

	return value_nil();
}

static void eval_stmt_let(env_t *e, stmt_t *stmt) {
	stmt_let_t *let = &stmt->as.let;

	size_t idx = -1;
	for (size_t i = 0; i < VARS_CAPACITY; ++ i) {
		if (e->vars[i].name == NULL) {
			if (idx == (size_t)-1)
				idx = i;
		} else if (strcmp(e->vars[i].name, let->name) == 0)
			fatal(stmt->where, "Variable '%s' redeclared", let->name);
	}

	if (idx == (size_t)-1)
		fatal(stmt->where, "Reached max limit of %i variables", VARS_CAPACITY);

	e->vars[idx].name = let->name;
	e->vars[idx].val  = eval_expr(e, let->val);
}

void eval(env_t *e, stmt_t *program) {
	for (stmt_t *stmt = program; stmt != NULL; stmt = stmt->next) {
		switch (stmt->type) {
		case STMT_TYPE_EXPR: eval_expr(e, stmt->as.expr); break;
		case STMT_TYPE_LET:  eval_stmt_let(e, stmt);      break;

		default: UNREACHABLE("Unknown statement type");
		}
	}
}
