#include "eval.h"

static const char *value_type_to_cstr_map[] = {
	[VALUE_TYPE_NIL]  = "nil",
	[VALUE_TYPE_STR]  = "string",
	[VALUE_TYPE_NUM]  = "number",
	[VALUE_TYPE_BOOL] = "boolean",
};

static_assert(VALUE_TYPE_COUNT == 4); /* Add the new value type to the map */

const char *value_type_to_cstr(value_type_t type) {
	if (type >= VALUE_TYPE_COUNT)
		UNREACHABLE("Invalid value type");

	return value_type_to_cstr_map[type];
}

static void wrong_type(where_t where, value_type_t type, const char *in) {
	fatal(where, "Wrong type '%s' in %s", value_type_to_cstr(type), in);
}

static void wrong_arg_count(where_t where, size_t got, size_t expected) {
	fatal(where, "Expected '%zu' argument(s), got '%zu'", expected, got);
}

value_t value_nil(void) {
	return (value_t){.type = VALUE_TYPE_NIL};
}

static value_t eval_expr(expr_t *expr);

static value_t builtin_print(expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	for (size_t i = 0; i < call->args_count; ++ i) {
		if (i > 0)
			putchar(' ');

		value_t value = eval_expr(call->args[i]);
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

static value_t builtin_println(expr_t *expr) {
	builtin_print(expr);
	putchar('\n');

	return value_nil();
}

static value_t builtin_len(expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	switch (call->args[0]->type) {
	case EXPR_TYPE_STR:
		return (value_t){
			.type   = VALUE_TYPE_NUM,
			.as.num = strlen(call->args[0]->as.str),
		};

	default: wrong_type(expr->where, call->args[0]->type, "'len' function");
	}

	return value_nil();
}

static builtin_t builtins[] = {
	{.name = "println", .func = builtin_println},
	{.name = "print",   .func = builtin_print},
	{.name = "len",     .func = builtin_len},
};

static value_t eval_expr_call(expr_t *expr) {
	for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); ++ i) {
		if (strcmp(builtins[i].name, expr->as.call.name) == 0)
			return builtins[i].func(expr);
	}

	fatal(expr->where, "Unknown function '%s'", expr->as.call.name);
	return value_nil();
}

static value_t eval_expr_str(expr_t *expr) {
	value_t value;
	value.type   = VALUE_TYPE_STR;
	value.as.str = expr->as.str;

	return value;
}

static value_t eval_expr_num(expr_t *expr) {
	value_t value;
	value.type   = VALUE_TYPE_NUM;
	value.as.num = expr->as.num;

	return value;
}

static value_t eval_expr_bool(expr_t *expr) {
	value_t value;
	value.type     = VALUE_TYPE_BOOL;
	value.as.bool_ = expr->as.bool_;

	return value;
}

static value_t eval_expr_bin_op_equals(expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(bin_op->left);
	value_t right = eval_expr(bin_op->right);

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

static value_t eval_expr_bin_op(expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	switch (bin_op->type) {
	case TOKEN_TYPE_EQUALS: return eval_expr_bin_op_equals(expr);
	case TOKEN_TYPE_NOT_EQUALS: {
		value_t value  = eval_expr_bin_op_equals(expr);
		value.as.bool_ = !value.as.bool_;
		return value;
	}

	case TOKEN_TYPE_ADD: {
		value_t left  = eval_expr(bin_op->left);
		value_t right = eval_expr(bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '+' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '+' operation, expectedcted same as left side");

		left.as.num += right.as.num;
		return left;
	}

	case TOKEN_TYPE_SUB: {
		value_t left  = eval_expr(bin_op->left);
		value_t right = eval_expr(bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '-' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '-' operation, expected same as left side");

		left.as.num -= right.as.num;
		return left;
	}

	case TOKEN_TYPE_MUL: {
		value_t left  = eval_expr(bin_op->left);
		value_t right = eval_expr(bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '*' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '*' operation, expected same as left side");

		left.as.num *= right.as.num;
		return left;
	}

	case TOKEN_TYPE_DIV: {
		value_t left  = eval_expr(bin_op->left);
		value_t right = eval_expr(bin_op->right);

		if (left.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, left.type, "left side of '/' operation");
		else if (right.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, right.type,
			           "right side of '/' operation, expected same as left side");

		left.as.num /= right.as.num;
		return left;
	}

	case TOKEN_TYPE_POW: {
		value_t left  = eval_expr(bin_op->left);
		value_t right = eval_expr(bin_op->right);

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

static value_t eval_expr(expr_t *expr) {
	switch (expr->type) {
	case EXPR_TYPE_CALL:   return eval_expr_call(expr);
	case EXPR_TYPE_STR:    return eval_expr_str(expr);
	case EXPR_TYPE_NUM:    return eval_expr_num(expr);
	case EXPR_TYPE_BOOL:   return eval_expr_bool(expr);
	case EXPR_TYPE_BIN_OP: return eval_expr_bin_op(expr);

	default: UNREACHABLE("Unknown expression type");
	}

	return value_nil();
}

void eval(stmt_t *program) {
	for (stmt_t *stmt = program; stmt != NULL; stmt = stmt->next) {
		switch (stmt->type) {
		case STMT_TYPE_EXPR: eval_expr(stmt->as.expr); break;

		default: UNREACHABLE("Unknown statement type");
		}
	}
}
