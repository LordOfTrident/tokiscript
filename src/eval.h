#ifndef EVAL_H__HEADER_GUARD__
#define EVAL_H__HEADER_GUARD__

#include <stdio.h>  /* printf, putchar */
#include <string.h> /* strcmp, strlen */
#include <assert.h> /* static_assert */
#include <math.h>   /* pow */

#include "parser.h"
#include "node.h"

typedef enum {
	VALUE_TYPE_NIL = 0,
	VALUE_TYPE_STR,
	VALUE_TYPE_NUM,
	VALUE_TYPE_BOOL,

	VALUE_TYPE_COUNT,
} value_type_t;

const char *value_type_to_cstr(value_type_t p_type);

typedef struct {
	value_type_t type;

	union {
		double num;
		bool   bool_;
		char  *str;
	} as;
} value_t;

static_assert(VALUE_TYPE_COUNT == 4); /* Add new values to union */

value_t value_nil(void);

typedef value_t (*builtin_func_t)(expr_t *p_expr);

typedef struct {
	const char    *name;
	builtin_func_t func;
} builtin_t;

void eval(stmt_t *p_program);

#endif
