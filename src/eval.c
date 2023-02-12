#include "eval.h"

void env_init(env_t *e) {
	memset(e, 0, sizeof(*e));
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

	error(expr->where, "Unknown function '%s'", expr->as.call.name);
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
		undefined(expr->where, expr->as.id.name);

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

static value_t eval_expr_bin_op_not_equals(env_t *e, expr_t *expr) {
	value_t val  = eval_expr_bin_op_equals(e, expr);
	val.as.bool_ = !val.as.bool_;
	return val;
}

static value_t eval_expr_bin_op_assign(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->left->type != EXPR_TYPE_ID)
		error(expr->where, "left side of '=' expected variable");

	char *name = bin_op->left->as.id.name;

	value_t val = eval_expr(e, bin_op->right);
	var_t *var  = env_get_var(e, name);
	if (var == NULL)
		undefined(expr->where, name);

	if (val.type != var->val.type)
		wrong_type(expr->where, val.type, "assignment");

	var->val = val;
	return val;
}

static value_t eval_expr_bin_op_add(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->assign) {
		if (bin_op->left->type != EXPR_TYPE_ID)
			error(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t *var  = env_get_var(e, name);
		if (var == NULL)
			undefined(expr->where, name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "'+' assignment");

		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "left side of '+' assignment");

		var->val.as.num += val.as.num;
		return val;
	}

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '+' operation");
	else if (right.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, right.type,
		           "right side of '+' operation, expected same as left side");

	left.as.num += right.as.num;
	return left;
}

static value_t eval_expr_bin_op_sub(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->assign) {
		if (bin_op->left->type != EXPR_TYPE_ID)
			error(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t *var  = env_get_var(e, name);
		if (var == NULL)
			undefined(expr->where, name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "'-' assignment");

		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "left side of '-' assignment");

		var->val.as.num -= val.as.num;
		return val;
	}

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

static value_t eval_expr_bin_op_mul(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->assign) {
		if (bin_op->left->type != EXPR_TYPE_ID)
			error(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t *var  = env_get_var(e, name);
		if (var == NULL)
			undefined(expr->where, name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "'*' assignment");

		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "left side of '*' assignment");

		var->val.as.num *= val.as.num;
		return val;
	}

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

static value_t eval_expr_bin_op_div(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->assign) {
		if (bin_op->left->type != EXPR_TYPE_ID)
			error(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t *var  = env_get_var(e, name);
		if (var == NULL)
			undefined(expr->where, name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "'/' assignment");

		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "left side of '/' assignment");

		var->val.as.num /= val.as.num;
		return val;
	}

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

static value_t eval_expr_bin_op_pow(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->assign) {
		if (bin_op->left->type != EXPR_TYPE_ID)
			error(expr->where, "left side of '=' expected variable");

		char *name = bin_op->left->as.id.name;

		value_t val = eval_expr(e, bin_op->right);
		var_t *var  = env_get_var(e, name);
		if (var == NULL)
			undefined(expr->where, name);

		if (val.type != var->val.type)
			wrong_type(expr->where, val.type, "'^' assignment");

		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "left side of '^' assignment");

		var->val.as.num = pow(var->val.as.num, val.as.num);
		return val;
	}

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

static value_t eval_expr_bin_op(env_t *e, expr_t *expr) {
	switch (expr->as.bin_op.type) {
	case BIN_OP_EQUALS:     return eval_expr_bin_op_equals(    e, expr);
	case BIN_OP_NOT_EQUALS: return eval_expr_bin_op_not_equals(e, expr);

	case BIN_OP_ASSIGN: return eval_expr_bin_op_assign(e, expr);

	case BIN_OP_ADD: return eval_expr_bin_op_add(e, expr);
	case BIN_OP_SUB: return eval_expr_bin_op_sub(e, expr);
	case BIN_OP_MUL: return eval_expr_bin_op_mul(e, expr);
	case BIN_OP_DIV: return eval_expr_bin_op_div(e, expr);
	case BIN_OP_POW: return eval_expr_bin_op_pow(e, expr);

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
			error(stmt->where, "Variable '%s' redeclared", let->name);
	}

	if (idx == (size_t)-1)
		error(stmt->where, "Reached max limit of %i variables", VARS_CAPACITY);

	e->vars[idx].name = let->name;
	e->vars[idx].val  = eval_expr(e, let->val);
}

static void eval_stmt_if(env_t *e, stmt_t *stmt) {
	stmt_if_t *if_ = &stmt->as.if_;

	value_t cond = eval_expr(e, if_->cond);
	if (cond.type != VALUE_TYPE_BOOL)
		wrong_type(stmt->where, cond.type, "if statement condition");

	if (cond.as.bool_)
		eval(e, if_->body);
}

void eval(env_t *e, stmt_t *program) {
	for (stmt_t *stmt = program; stmt != NULL; stmt = stmt->next) {
		switch (stmt->type) {
		case STMT_TYPE_EXPR: eval_expr(e, stmt->as.expr); break;
		case STMT_TYPE_LET:  eval_stmt_let(e, stmt);      break;
		case STMT_TYPE_IF:   eval_stmt_if( e, stmt);      break;

		default: UNREACHABLE("Unknown statement type");
		}
	}
}
