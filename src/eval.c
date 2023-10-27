#include "eval.h"

static void env_scope_begin(env_t *e) {
	if (e->scope == NULL)
		e->scope = e->scopes;
	else
		/* TODO: Possible buffer overflow */
		++ e->scope;

	e->scope->vars_count = 0;
	if (e->scope->vars == NULL) {
		e->scope->vars_cap = VARS_CHUNK;
		e->scope->vars     = (var_t*)malloc(e->scope->vars_cap * sizeof(var_t));
		if (e->scope->vars == NULL)
			UNREACHABLE("malloc() fail");
	}

	e->scope->defer_count = 0;
	if (e->scope->defer == NULL) {
		e->scope->defer_cap = DEFER_CHUNK;
		e->scope->defer     = (stmt_t**)malloc(e->scope->defer_cap * sizeof(stmt_t*));
		if (e->scope->defer == NULL)
			UNREACHABLE("malloc() fail");
	}
}

static void env_scope_end(env_t *e) {
	for (size_t i = e->scope->defer_count; i --> 0;)
		eval(e, e->scope->defer[i]);

	-- e->scope;
}

static var_t *env_new_var(env_t *e, const char *name, bool const_) {
	size_t idx = -1;
	for (size_t i = 0; i < e->scope->vars_count; ++ i) {
		if (e->scope->vars[i].name == NULL) {
			if (idx == (size_t)-1)
				idx = i;
		} else if (strcmp(e->scope->vars[i].name, name) == 0)
			return NULL;
	}

	if (idx == (size_t)-1) {
		if (e->scope->vars_count >= e->scope->vars_cap) {
			e->scope->vars_cap *= 2;
			e->scope->vars = (var_t*)realloc(e->scope->vars, e->scope->vars_cap * sizeof(var_t));
			if (e->scope->vars == NULL)
				UNREACHABLE("realloc() fail");
		}

		idx = e->scope->vars_count ++;
	}

	e->scope->vars[idx].name   = (char*)name;
	e->scope->vars[idx].const_ = const_;
	return e->scope->vars + idx;
}

static var_t *env_get_var(env_t *e, char *name) {
	for (scope_t *scope = e->scope; scope != e->scopes - 1; -- scope) {
		for (size_t i = 0; i < scope->vars_count; ++ i) {
			if (scope->vars[i].name == NULL)
				continue;

			if (strcmp(scope->vars[i].name, name) == 0)
				return &scope->vars[i];
		}
	}

	return NULL;
}

void env_init(env_t *e, int argc, const char **argv) {
	memset(e, 0, sizeof(*e));

	e->argc = argc;
	e->argv = argv;

	env_scope_begin(e);

	for (size_t i = 0; i < sizeof(builtins) / sizeof(*builtins); ++ i) {
		var_t *var = env_new_var(e, builtins[i].name, true);
		assert(var != NULL);

		var->val = value_nat(builtins[i].func);
	}
}

void env_deinit(env_t *e) {
	env_scope_end(e);
	for (size_t i = 0; i < MAX_NEST; ++ i) {
		if (e->scopes[i].vars != NULL)
			free(e->scopes[i].vars);

		if (e->scopes[i].defer != NULL)
			free(e->scopes[i].defer);
	}
}

static value_t eval_expr(env_t *e, expr_t *expr);

static void fprint_value(value_t value, FILE *file) {
	switch (value.type) {
	case VALUE_TYPE_NAT:  fprintf(file, "(native)"); break;
	case VALUE_TYPE_ARR:  fprintf(file, "(list %p)", (void*)value.as.arr.buf);   break;
	case VALUE_TYPE_FUN:  fprintf(file, "(fun %p)",  (void*)value.as.fun);       break;
	case VALUE_TYPE_NIL:  fprintf(file, "(nil)");                                break;
	case VALUE_TYPE_STR:  fprintf(file, "%s", value.as.str);                     break;
	case VALUE_TYPE_BOOL: fprintf(file, "%s", value.as.bool_? "true" : "false"); break;
	case VALUE_TYPE_NUM: {
		char buf[64] = {0};
		double_to_str(value.as.num, buf, sizeof(buf));
		fprintf(file, "%s", buf);
	} break;

	default: UNREACHABLE("Unknown value type");
	}
}

static value_t builtin_print(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	for (size_t i = 0; i < call->args_count; ++ i) {
		if (i > 0)
			putchar(' ');

		fprint_value(eval_expr(e, call->args[i]), stdout);
	}

	return value_nil();
}

static value_t builtin_println(env_t *e, expr_t *expr) {
	builtin_print(e, expr);
	putchar('\n');

	return value_nil();
}

static value_t builtin_panic(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	color_bold(stderr);
	fprintf(stderr, "%s:%i:%i: ", expr->where.path, expr->where.row, expr->where.col);
	color_fg(stderr, COLOR_BRED);
	fprintf(stderr, "panic():");
	color_reset(stderr);

	for (size_t i = 0; i < call->args_count; ++ i) {
		fputc(' ', stderr);
		fprint_value(eval_expr(e, call->args[i]), stderr);
	}

	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

static value_t builtin_len(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	/* TODO: Make length be stored with the string pointer, so its faster */
	value_t val = eval_expr(e, call->args[0]);
	switch (val.type) {
	case VALUE_TYPE_STR: return value_num(strlen(val.as.str));
	case VALUE_TYPE_ARR: return value_num(val.as.arr.size);

	default: wrong_type(expr->where, val.type, "'len' function");
	}

	return value_nil();
}

static value_t builtin_readnum(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	for (size_t i = 0; i < call->args_count; ++ i) {
		if (i > 0)
			putchar(' ');

		fprint_value(eval_expr(e, call->args[i]), stdout);
	}

	putchar(' ');

	char buf[1024] = {0};
	{
		char *_ = fgets(buf, sizeof(buf), stdin);
		UNUSED(_);
	}

	double val = 0;
	{
		int _ = sscanf(buf, "%lf", &val);
		UNUSED(_);
	}

	return value_num(val);
}

static value_t builtin_readstr(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	for (size_t i = 0; i < call->args_count; ++ i) {
		if (i > 0)
			putchar(' ');

		fprint_value(eval_expr(e, call->args[i]), stdout);
	}

	putchar(' ');

	char buf[1024] = {0};
	{
		char *_ = fgets(buf, sizeof(buf), stdin);
		(void)_;
	}

	size_t len = strlen(buf);
	if (len > 0) {
		if (buf[len - 1] == '\n')
			buf[len - 1] = '\0';
	}

	/* TODO: Solve memory leaks */
	return value_str(strcpy_to_heap(buf));
}

static value_t builtin_exit(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "'exit' function");

	exit((int)val.as.num);
}

static value_t builtin_system(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	size_t cap = 64, size = 0;
	char *str = (char*)malloc(cap);
	if (str == NULL)
		UNREACHABLE("malloc() fail");

	*str = '\0';
	for (size_t i = 0; i < call->args_count; ++ i) {
		value_t value   = eval_expr(e, call->args[i]);
		char    buf[64] = {0};
		const char *add = NULL;

		switch (value.type) {
		case VALUE_TYPE_NAT: add = "(native)"; break;
		case VALUE_TYPE_FUN:
			sprintf(buf, "(fun %p)", (void*)value.as.fun);
			add = buf;
			break;

		case VALUE_TYPE_ARR:
			sprintf(buf, "(list %p)", (void*)value.as.arr.buf);
			add = buf;
			break;

		case VALUE_TYPE_NIL:  add = "(nil)";      break;
		case VALUE_TYPE_STR:  add = value.as.str; break;
		case VALUE_TYPE_BOOL: add = value.as.bool_? "true" : "false"; break;
		case VALUE_TYPE_NUM:
			double_to_str(value.as.num, buf, sizeof(buf));
			add = buf;
			break;

		default: UNREACHABLE("Unknown value type");
		}

		size_t len = strlen(add);

		size += len;
		if (size + 1 >= cap) {
			do {
				cap *= 2;
			} while (size + 1 >= cap);

			str = (char*)realloc(str, cap);
			if (str == NULL)
				UNREACHABLE("realloc() fail");
		}

		strcat(str, add);
	}

	int result = system(str);
	free(str);
	return value_num(result);
}

static value_t builtin_platform(env_t *e, expr_t *expr) {
	UNUSED(e);
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 0)
		wrong_arg_count(expr->where, call->args_count, 0);

#if defined(WIN32)
	return value_str("windows");
#elif defined(__APPLE__)
	return value_str("apple");
#elif defined(__linux__) || defined(__gnu_linux__) || defined(linux)
	return value_str("linux");
#elif defined(__unix__) || defined(unix)
	return value_str("unix");
#else
	return value_str("unknown");
#endif
}

static value_t builtin_argc(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 0)
		wrong_arg_count(expr->where, call->args_count, 0);

	return value_num(e->argc);
}

static value_t builtin_argat(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "'argat' function");

	return value_str((char*)e->argv[(int)val.as.num]);
}

static value_t builtin_strtonum(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	if (val.type != VALUE_TYPE_STR)
		wrong_type(expr->where, val.type, "'strtonum' function");

	char *ptr;
	double n = (double)strtod(val.as.str, &ptr);
	if (*ptr != '\0')
		return value_nil();
	else
		return value_num(n);
}

static value_t builtin_numtostr(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "'numtostr' function");

	char buf[64] = {0};
	double_to_str(val.as.num, buf, sizeof(buf));
	return value_str(strcpy_to_heap(buf));
}

static value_t builtin_getenv(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	value_t val = eval_expr(e, call->args[0]);
	if (val.type != VALUE_TYPE_STR)
		wrong_type(expr->where, val.type, "'getenv' function");

	char *str = getenv(val.as.str);
	if (str == NULL)
		return value_nil();
	else
		return value_str(str);
}

static value_t builtin_type(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;

	if (call->args_count != 1)
		wrong_arg_count(expr->where, call->args_count, 1);

	return value_str((char*)value_type_to_cstr(eval_expr(e, call->args[0]).type));
}

builtin_t builtins[BUILTINS_COUNT] = {
	{.name = "println",  .func = builtin_println},
	{.name = "print",    .func = builtin_print},
	{.name = "len",      .func = builtin_len},
	{.name = "readnum",  .func = builtin_readnum},
	{.name = "readstr",  .func = builtin_readstr},
	{.name = "panic",    .func = builtin_panic},
	{.name = "exit",     .func = builtin_exit},
	{.name = "system",   .func = builtin_system},
	{.name = "platform", .func = builtin_platform},
	{.name = "argc",     .func = builtin_argc},
	{.name = "argat",    .func = builtin_argat},
	{.name = "strtonum", .func = builtin_strtonum},
	{.name = "numtostr", .func = builtin_numtostr},
	{.name = "getenv",   .func = builtin_getenv},
	{.name = "type",     .func = builtin_type},
};

static_assert(BUILTINS_COUNT == 15); /* Update builtins count */

static value_t eval_with_return(env_t *e, stmt_t *stmt) {
	++ e->returns;

	e->return_ = value_nil();
	eval(e, stmt);
	if (e->returning)
		e->returning = false;

	-- e->returns;
	return e->return_;
}

static value_t eval_expr_call(env_t *e, expr_t *expr) {
	expr_call_t *call = &expr->as.call;
	value_t to_call = eval_expr(e, call->expr);
	switch (to_call.type) {
	case VALUE_TYPE_NAT: return to_call.as.nat(e, expr);
	case VALUE_TYPE_FUN: {
		expr_fun_t *fun = (expr_fun_t*)to_call.as.fun;

		value_t evaled[ARGS_CAPACITY];
		for (size_t i = 0; i < fun->args_count; ++ i)
			evaled[i] = eval_expr(e, call->args[i]);

		env_scope_begin(e);

		for (size_t i = 0; i < fun->args_count; ++ i) {
			var_t *var = env_new_var(e, fun->args[i], false);
			var->val = evaled[i];
		}

		value_t val = eval_with_return(e, fun->body);

		env_scope_end(e);
		return val;
	}

	default: wrong_type(expr->where, to_call.type, "'()' operation");
	}

	return value_nil();
}

static value_t eval_expr_arr(env_t *e, expr_t *expr) {
	expr_arr_t *arr = &expr->as.arr;

	value_t val = value_arr(arr->size);
	for (size_t i = 0; i < arr->size; ++ i)
		val.as.arr.buf[i] = eval_expr(e, arr->buf[i]);

	return val;
}

static value_t eval_expr_fmt(env_t *e, expr_t *expr) {
	expr_fmt_t *fmt = &expr->as.fmt;

	size_t cap = 64, size = 0;
	char *str = (char*)malloc(cap);
	if (str == NULL)
		UNREACHABLE("malloc() fail");

	*str = '\0';
	size_t arg  = 0;
	char   prev = '\0';
	for (size_t i = 0; i < strlen(fmt->str); ++ i) {
		char    buf[64] = {0};
		const char *add = NULL;

		if (prev != '%' && fmt->str[i] == '%' && fmt->str[i + 1] == 'v') {
			if (arg >= fmt->args_count)
				error(expr->where, "Unexpected string format at index %i", (int)i);

			value_t value = eval_expr(e, fmt->args[arg ++]);

			switch (value.type) {
			case VALUE_TYPE_NAT: add = "(native)"; break;
			case VALUE_TYPE_FUN:
				sprintf(buf, "(fun %p)", (void*)value.as.fun);
				add = buf;
				break;

			case VALUE_TYPE_ARR:
				sprintf(buf, "(list %p)", (void*)value.as.arr.buf);
				add = buf;
				break;

			case VALUE_TYPE_NIL:  add = "(nil)";      break;
			case VALUE_TYPE_STR:  add = value.as.str; break;
			case VALUE_TYPE_BOOL: add = value.as.bool_? "true" : "false"; break;
			case VALUE_TYPE_NUM:
				double_to_str(value.as.num, buf, sizeof(buf));
				add = buf;
				break;

			default: UNREACHABLE("Unknown value type");
			}

			++ i;
		} else {
			buf[0] = fmt->str[i];
			buf[1] = '\0';
			add = buf;
		}

		size_t len = strlen(add);

		size += len;
		if (size + 1 >= cap) {
			do {
				cap *= 2;
			} while (size + 1 >= cap);

			str = (char*)realloc(str, cap);
			if (str == NULL)
				UNREACHABLE("realloc() fail");
		}

		strcat(str, add);
		prev = fmt->str[i];
	}

	if (arg < fmt->args_count)
		error(fmt->args[arg]->where, "Unexpected format argument");

	return value_str(str);
}

static value_t eval_expr_idx(env_t *e, expr_t *expr) {
	expr_idx_t *idx = &expr->as.idx;
	value_t to_idx = eval_expr(e, idx->expr);

	if (idx->end != NULL) {
		value_t start = eval_expr(e, idx->start);
		if (start.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, start.type, "'[]' operation start index");

		int startPos = (int)start.as.num;
		if (startPos < 0)
			error(expr->where, "Negative start index is not allowed");

		value_t end = eval_expr(e, idx->end);
		if (end.type != VALUE_TYPE_NUM && end.type != VALUE_TYPE_NIL)
			wrong_type(expr->where, end.type, "'[]' operation end index");

		int endPos = end.type == VALUE_TYPE_NIL? 0 : (int)end.as.num;
		if (endPos < 0)
			error(expr->where, "Negative end index is not allowed");

		if (end.type != VALUE_TYPE_NIL && startPos > endPos) {
			int tmp  = endPos;
			endPos   = startPos;
			startPos = tmp;
		}

		switch (to_idx.type) {
		case VALUE_TYPE_STR: {
			size_t len = strlen(to_idx.as.str);
			if ((size_t)startPos >= len)
				error(expr->where, "Start index exceeds string length");
			else if ((size_t)endPos >= len)
				error(expr->where, "End index exceeds string length");

			/* Very lazy */
			char *buf = strcpy_to_heap(to_idx.as.str + startPos);
			if (end.type != VALUE_TYPE_NIL)
				buf[endPos - startPos] = '\0';
			return value_str(buf);
		}

		case VALUE_TYPE_ARR: {
			size_t size = to_idx.as.arr.size;
			if ((size_t)startPos >= size)
				error(expr->where, "Start index exceeds array length");
			else if ((size_t)endPos >= size)
				error(expr->where, "End index exceeds array length");

			size = end.type == VALUE_TYPE_NIL? size - startPos : (size_t)endPos - startPos;

			value_t val = value_arr(size);
			for (size_t i = 0; i < val.as.arr.size; ++ i)
				val.as.arr.buf[i] = to_idx.as.arr.buf[startPos + i];

			return val;
		}

		default: wrong_type(expr->where, to_idx.type, "'[]' operation");
		}
	} else {
		value_t val = eval_expr(e, idx->start);
		if (val.type != VALUE_TYPE_NUM)
			wrong_type(expr->where, val.type, "'[]' operation index");

		int pos = (int)val.as.num;
		if (pos < 0)
			error(expr->where, "Negative index is not allowed");

		switch (to_idx.type) {
		case VALUE_TYPE_STR: {
			if ((size_t)pos >= strlen(to_idx.as.str))
				error(expr->where, "Index exceeds string length");

			char buf[] = {to_idx.as.str[pos], '\0'};
			return value_str(strcpy_to_heap(buf));
		}

		case VALUE_TYPE_ARR: return to_idx.as.arr.buf[pos];

		default: wrong_type(expr->where, to_idx.type, "'[]' operation");
		}
	}

	return value_nil();
}

static value_t eval_expr_id(env_t *e, expr_t *expr) {
	var_t *var = env_get_var(e, expr->as.id.name);
	if (var == NULL)
		undefined(expr->where, expr->as.id.name);

	return var->val;
}

static value_t eval_expr_do(env_t *e, expr_t *expr) {
	expr_do_t *do_ = &expr->as.do_;
	env_scope_begin(e);

	value_t value = eval_with_return(e, do_->body);

	env_scope_end(e);
	return value;
}

static value_t eval_expr_fun(env_t *e, expr_t *expr) {
	UNUSED(e);
	expr_fun_t *fun = &expr->as.fun;
	return value_fun(fun);
}

static value_t eval_expr_value(env_t *e, expr_t *expr) {
	UNUSED(e);
	return expr->as.val;
}

static int values_are_equal(value_t left, value_t right) {
	if (right.type != left.type)
		return false;

	switch (left.type) {
	case VALUE_TYPE_NUM:  return left.as.num   == right.as.num;
	case VALUE_TYPE_BOOL: return left.as.bool_ == right.as.bool_;
	case VALUE_TYPE_STR:  return strcmp(left.as.str, right.as.str) == 0;
	case VALUE_TYPE_NIL:  return true;
	case VALUE_TYPE_FUN:  return left.as.fun     == right.as.fun;
	case VALUE_TYPE_NAT:  return left.as.nat     == right.as.nat;
	case VALUE_TYPE_ARR:  return left.as.arr.buf == right.as.arr.buf;

	default: UNREACHABLE("Unknown value type");
	}

	return false;
}

static value_t eval_expr_bin_op_equals(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	return value_bool(values_are_equal(left, right));
}

static value_t eval_expr_bin_op_not_equals(env_t *e, expr_t *expr) {
	value_t val  = eval_expr_bin_op_equals(e, expr);
	val.as.bool_ = !val.as.bool_;
	return val;
}

static value_t eval_expr_bin_op_greater(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, left.type,
		           "right side of '>' operation, expected same as left side");

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '>' operation");

	return value_bool(left.as.num > right.as.num);
}

static value_t eval_expr_bin_op_greater_equ(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, left.type,
		           "right side of '>=' operation, expected same as left side");

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '>=' operation");

	return value_bool(left.as.num >= right.as.num);
}

static value_t eval_expr_bin_op_less(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, left.type,
		           "right side of '<' operation, expected same as left side");

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '<' operation");

	return value_bool(left.as.num < right.as.num);
}

static value_t eval_expr_bin_op_less_equ(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, left.type,
		           "right side of '<=' operation, expected same as left side");

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '<=' operation");

	return value_bool(left.as.num <= right.as.num);
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

	if (var->const_)
		error(expr->where, "Attempt to assign to constant '%s'", name);

	var->val = val;
	return val;
}

static value_t eval_expr_bin_op_inc(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->left->type != EXPR_TYPE_ID)
		error(expr->where, "left side of '++' expected variable");

	char *name = bin_op->left->as.id.name;

	value_t val = eval_expr(e, bin_op->right);
	var_t *var  = env_get_var(e, name);
	if (var == NULL)
		undefined(expr->where, name);

	if (val.type != var->val.type)
		wrong_type(expr->where, val.type, "'++' assignment");

	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "left side of '++' assignment");

	var->val.as.num += val.as.num;
	return val;
}

static value_t eval_expr_bin_op_dec(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->left->type != EXPR_TYPE_ID)
		error(expr->where, "left side of '--' expected variable");

	char *name = bin_op->left->as.id.name;

	value_t val = eval_expr(e, bin_op->right);
	var_t *var  = env_get_var(e, name);
	if (var == NULL)
		undefined(expr->where, name);

	if (val.type != var->val.type)
		wrong_type(expr->where, val.type, "'--' assignment");

	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "left side of '--' assignment");

	var->val.as.num -= val.as.num;
	return val;
}

static value_t eval_expr_bin_op_xinc(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->left->type != EXPR_TYPE_ID)
		error(expr->where, "left side of '**' expected variable");

	char *name = bin_op->left->as.id.name;

	value_t val = eval_expr(e, bin_op->right);
	var_t *var  = env_get_var(e, name);
	if (var == NULL)
		undefined(expr->where, name);

	if (val.type != var->val.type)
		wrong_type(expr->where, val.type, "'**' assignment");

	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "left side of '**' assignment");

	var->val.as.num *= val.as.num;
	return val;
}

static value_t eval_expr_bin_op_xdec(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	if (bin_op->left->type != EXPR_TYPE_ID)
		error(expr->where, "left side of '//' expected variable");

	char *name = bin_op->left->as.id.name;

	value_t val = eval_expr(e, bin_op->right);
	var_t *var  = env_get_var(e, name);
	if (var == NULL)
		undefined(expr->where, name);

	if (val.type != var->val.type)
		wrong_type(expr->where, val.type, "'//' assignment");

	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "left side of '//' assignment");

	if (val.as.num == 0)
		error(expr->where, "division by zero");

	var->val.as.num /= val.as.num;
	return val;
}

static value_t eval_expr_bin_op_add(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type != left.type)
		wrong_type(expr->where, right.type,
		           "right side of '+' operation, expected same as left side");

	if (left.type == VALUE_TYPE_STR) {
		/* TODO: Solve memory leaks */
		char *concatted = (char*)malloc(strlen(left.as.str) + strlen(right.as.str) + 1);
		if (concatted == NULL)
			UNREACHABLE("malloc() fail");

		strcpy(concatted, left.as.str);
		strcat(concatted, right.as.str);
		left.as.str = concatted;
	} else if (left.type == VALUE_TYPE_NUM)
		left.as.num += right.as.num;
	else
		wrong_type(expr->where, left.type, "left side of '+' operation");

	return left;
}

static value_t eval_expr_bin_op_sub(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

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

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '/' operation");
	else if (right.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, right.type,
		           "right side of '/' operation, expected same as left side");

	if (right.as.num == 0)
		error(expr->where, "division by zero");

	left.as.num /= right.as.num;
	return left;
}

static value_t eval_expr_bin_op_pow(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

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

static value_t eval_expr_bin_op_mod(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (left.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, left.type, "left side of '^' operation");
	else if (right.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, right.type,
		           "right side of '^' operation, expected same as left side");

	if (right.as.num == 0)
		error(expr->where, "division by zero");

	float remainder = left.as.num / right.as.num;
	left.as.num = right.as.num * (remainder - floor(remainder));
	return left;
}

static value_t eval_expr_bin_op_and(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (left.type != VALUE_TYPE_BOOL)
		wrong_type(expr->where, left.type, "left side of 'and' operation");
	else if (right.type != VALUE_TYPE_BOOL)
		wrong_type(expr->where, right.type,
		           "right side of 'and' operation, expected same as left side");

	left.as.bool_ = left.as.bool_ && right.as.bool_;
	return left;
}

static value_t eval_expr_bin_op_or(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (left.type != VALUE_TYPE_BOOL)
		wrong_type(expr->where, left.type, "left side of 'or' operation");
	else if (right.type != VALUE_TYPE_BOOL)
		wrong_type(expr->where, right.type,
		           "right side of 'or' operation, expected same as left side");

	left.as.bool_ = left.as.bool_ || right.as.bool_;
	return left;
}

static value_t eval_expr_bin_op_in(env_t *e, expr_t *expr) {
	expr_bin_op_t *bin_op = &expr->as.bin_op;

	value_t left  = eval_expr(e, bin_op->left);
	value_t right = eval_expr(e, bin_op->right);

	if (right.type == VALUE_TYPE_ARR) {
		for (size_t i = 0; i < right.as.arr.size; ++ i) {
			if (values_are_equal(right.as.arr.buf[i], left))
				return value_num(i);
		}
	} else if (right.type == VALUE_TYPE_STR) {
		if (left.type != VALUE_TYPE_STR)
			wrong_type(expr->where, left.type, "left side of 'in' operation");

		const char *ptr = strstr(right.as.str, left.as.str);
		if (ptr != NULL)
			return value_num((double)(ptr - right.as.str));
	} else
		wrong_type(expr->where, right.type, "right side of 'in' operation");

	return value_nil();
}

static value_t eval_expr_bin_op(env_t *e, expr_t *expr) {
	switch (expr->as.bin_op.type) {
	case BIN_OP_EQUALS:      return eval_expr_bin_op_equals(     e, expr);
	case BIN_OP_NOT_EQUALS:  return eval_expr_bin_op_not_equals( e, expr);
	case BIN_OP_GREATER:     return eval_expr_bin_op_greater(    e, expr);
	case BIN_OP_GREATER_EQU: return eval_expr_bin_op_greater_equ(e, expr);
	case BIN_OP_LESS:        return eval_expr_bin_op_less(       e, expr);
	case BIN_OP_LESS_EQU:    return eval_expr_bin_op_less_equ(   e, expr);

	case BIN_OP_AND: return eval_expr_bin_op_and(e, expr);
	case BIN_OP_OR:  return eval_expr_bin_op_or( e, expr);
	case BIN_OP_IN:  return eval_expr_bin_op_in( e, expr);

	case BIN_OP_ASSIGN: return eval_expr_bin_op_assign(e, expr);
	case BIN_OP_INC:    return eval_expr_bin_op_inc(   e, expr);
	case BIN_OP_DEC:    return eval_expr_bin_op_dec(   e, expr);
	case BIN_OP_XINC:   return eval_expr_bin_op_xinc(  e, expr);
	case BIN_OP_XDEC:   return eval_expr_bin_op_xdec(  e, expr);

	case BIN_OP_ADD: return eval_expr_bin_op_add(e, expr);
	case BIN_OP_SUB: return eval_expr_bin_op_sub(e, expr);
	case BIN_OP_MUL: return eval_expr_bin_op_mul(e, expr);
	case BIN_OP_DIV: return eval_expr_bin_op_div(e, expr);
	case BIN_OP_POW: return eval_expr_bin_op_pow(e, expr);
	case BIN_OP_MOD: return eval_expr_bin_op_mod(e, expr);

	default: UNREACHABLE("Unknown binary operation type");
	}
}

static value_t eval_expr_un_op_pos(env_t *e, expr_t *expr) {
	expr_un_op_t *un_op = &expr->as.un_op;

	value_t val = eval_expr(e, un_op->expr);
	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "'+' unary operation");

	return val;
}

static value_t eval_expr_un_op_neg(env_t *e, expr_t *expr) {
	expr_un_op_t *un_op = &expr->as.un_op;

	value_t val = eval_expr(e, un_op->expr);
	if (val.type != VALUE_TYPE_NUM)
		wrong_type(expr->where, val.type, "'-' unary operation");

	return value_num(-val.as.num);
}

static value_t eval_expr_un_op_not(env_t *e, expr_t *expr) {
	expr_un_op_t *un_op = &expr->as.un_op;

	value_t val = eval_expr(e, un_op->expr);
	if (val.type != VALUE_TYPE_BOOL)
		wrong_type(expr->where, val.type, "'not' operation");

	return value_bool(!val.as.bool_);
}

static value_t eval_expr_un_op(env_t *e, expr_t *expr) {
	switch (expr->as.un_op.type) {
	case UN_OP_POS: return eval_expr_un_op_pos(e, expr);
	case UN_OP_NEG: return eval_expr_un_op_neg(e, expr);
	case UN_OP_NOT: return eval_expr_un_op_not(e, expr);

	default: UNREACHABLE("Unknown unary operation type");
	}
}

static value_t eval_expr(env_t *e, expr_t *expr) {
	switch (expr->type) {
	case EXPR_TYPE_CALL:   return eval_expr_call(  e, expr);
	case EXPR_TYPE_FMT:    return eval_expr_fmt(   e, expr);
	case EXPR_TYPE_ARR:    return eval_expr_arr(   e, expr);
	case EXPR_TYPE_IDX:    return eval_expr_idx(   e, expr);
	case EXPR_TYPE_ID:     return eval_expr_id(    e, expr);
	case EXPR_TYPE_DO:     return eval_expr_do(    e, expr);
	case EXPR_TYPE_FUN:    return eval_expr_fun(   e, expr);
	case EXPR_TYPE_VALUE:  return eval_expr_value( e, expr);
	case EXPR_TYPE_BIN_OP: return eval_expr_bin_op(e, expr);
	case EXPR_TYPE_UN_OP:  return eval_expr_un_op( e, expr);

	default: UNREACHABLE("Unknown expression type");
	}

	return value_nil();
}

static void eval_stmt_let(env_t *e, stmt_t *stmt) {
	stmt_let_t *let = &stmt->as.let;

	var_t *var = env_new_var(e, let->name, let->const_);
	if (var == NULL)
		error(stmt->where, "Variable '%s' redeclared", let->name);

	var->val = let->val == NULL? value_nil() : eval_expr(e, let->val);

	if (let->next != NULL)
		eval_stmt_let(e, let->next);
}

static void eval_stmt_if(env_t *e, stmt_t *stmt) {
	stmt_if_t *if_ = &stmt->as.if_;

	value_t cond = eval_expr(e, if_->cond);
	if (cond.type != VALUE_TYPE_BOOL)
		wrong_type(stmt->where, cond.type, "if statement condition");

	env_scope_begin(e);

	if (cond.as.bool_)
		eval(e, if_->body);
	else if (if_->next != NULL)
		eval_stmt_if(e, if_->next);
	else
		eval(e, if_->else_);

	env_scope_end(e);
}

static void eval_stmt_while(env_t *e, stmt_t *stmt) {
	stmt_while_t *while_ = &stmt->as.while_;

	++ e->breaks;
	while (true) {
		value_t cond = eval_expr(e, while_->cond);
		if (cond.type != VALUE_TYPE_BOOL)
			wrong_type(stmt->where, cond.type, "while statement condition");

		if (!cond.as.bool_)
			break;

		env_scope_begin(e);
		eval(e, while_->body);
		env_scope_end(e);
		if (e->returning)
			break;
		else if (e->breaking) {
			e->breaking = false;
			break;
		} else if (e->continuing)
			e->continuing = false;
	}
	-- e->breaks;
}

static void eval_stmt_for(env_t *e, stmt_t *stmt) {
	stmt_for_t *for_ = &stmt->as.for_;
	env_scope_begin(e);

	eval(e, for_->init);
	if (e->returning)
		error(stmt->where, "Unexpected return in for loop");

	++ e->breaks;
	while (true) {
		value_t cond = eval_expr(e, for_->cond);
		if (cond.type != VALUE_TYPE_BOOL)
			wrong_type(stmt->where, cond.type, "for statement condition");

		if (cond.as.bool_) {
			env_scope_begin(e);
			eval(e, for_->body);
			env_scope_end(e);
			if (e->returning)
				break;
			else if (e->breaking) {
				e->breaking = false;
				break;
			} else if (e->continuing)
				e->continuing = false;

			eval(e, for_->step);
			if (e->returning)
				error(stmt->where, "Unexpected return in for loop step");
			else if (e->breaking)
				error(stmt->where, "Unexpected break in for loop step");
			else if (e->continuing)
				error(stmt->where, "Unexpected continue in for loop step");
		} else
			break;
	}
	-- e->breaks;

	env_scope_end(e);
}

static void eval_stmt_return(env_t *e, stmt_t *stmt) {
	if (e->returns == 0)
		error(stmt->where, "Unexpected return");

	stmt_return_t *return_ = &stmt->as.return_;
	e->return_   = eval_expr(e, return_->expr);
	e->returning = true;
}

static void eval_stmt_break(env_t *e, stmt_t *stmt) {
	if (e->breaks == 0)
		error(stmt->where, "Unexpected break");

	e->breaking = true;
}

static void eval_stmt_continue(env_t *e, stmt_t *stmt) {
	if (e->breaks == 0)
		error(stmt->where, "Unexpected continue");

	e->continuing = true;
}

static void eval_stmt_defer(env_t *e, stmt_t *stmt) {
	stmt_defer_t *defer = &stmt->as.defer;

	if (e->scope->defer_count >= e->scope->defer_cap) {
		e->scope->defer_cap *= 2;
		e->scope->defer = (stmt_t**)realloc(e->scope->defer, e->scope->defer_cap * sizeof(stmt_t*));
		if (e->scope->defer == NULL)
			UNREACHABLE("realloc() fail");
	}

	e->scope->defer[e->scope->defer_count ++] = defer->stmt;
}

static void eval_stmt_fun(env_t *e, stmt_t *stmt) {
	stmt_fun_t *fun = &stmt->as.fun;

	var_t *var = env_new_var(e, fun->name, true);
	if (var == NULL)
		error(stmt->where, "Function '%s' redeclared", fun->name);

	var->val = eval_expr(e, fun->def);
}

void eval(env_t *e, stmt_t *program) {
	for (stmt_t *stmt = program; stmt != NULL; stmt = stmt->next) {
		switch (stmt->type) {
		case STMT_TYPE_EXPR:     eval_expr(         e, stmt->as.expr); break;
		case STMT_TYPE_LET:      eval_stmt_let(     e, stmt);          break;
		case STMT_TYPE_IF:       eval_stmt_if(      e, stmt);          break;
		case STMT_TYPE_WHILE:    eval_stmt_while(   e, stmt);          break;
		case STMT_TYPE_FOR:      eval_stmt_for(     e, stmt);          break;
		case STMT_TYPE_RETURN:   eval_stmt_return(  e, stmt);          return;
		case STMT_TYPE_BREAK:    eval_stmt_break(   e, stmt);          return;
		case STMT_TYPE_CONTINUE: eval_stmt_continue(e, stmt);          return;
		case STMT_TYPE_DEFER:    eval_stmt_defer(   e, stmt);          break;
		case STMT_TYPE_FUN:      eval_stmt_fun(     e, stmt);          break;

		default: UNREACHABLE("Unknown statement type");
		}
	}
}
