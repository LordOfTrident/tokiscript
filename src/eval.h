#ifndef EVAL_H_HEADER_GUARD
#define EVAL_H_HEADER_GUARD

#include <stdio.h>  /* printf, putchar */
#include <string.h> /* strcmp, strlen, memset */
#include <assert.h> /* static_assert */
#include <math.h>   /* pow */

#include "error.h"
#include "parser.h"
#include "value.h"
#include "node.h"

typedef struct {
	char   *name;
	value_t val;
} var_t;

#define VARS_CHUNK 32
#define MAX_NEST   64

typedef struct {
	var_t *vars;
	size_t vars_count, vars_cap;
} scope_t;

typedef struct {
	scope_t scopes[MAX_NEST], *scope;
} env_t;

typedef value_t (*builtin_func_t)(env_t*, expr_t *expr);

typedef struct {
	const char    *name;
	builtin_func_t func;
} builtin_t;

void env_init(  env_t *e);
void env_deinit(env_t *e);

void eval(env_t *e, stmt_t *program);

#endif
