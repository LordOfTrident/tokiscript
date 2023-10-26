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

#define VARS_CHUNK  32
#define DEFER_CHUNK 8
#define MAX_NEST    64

typedef struct {
	var_t *vars;
	size_t vars_count, vars_cap;

	stmt_t **defer;
	size_t   defer_count, defer_cap;
} scope_t;

typedef struct {
	scope_t scopes[MAX_NEST], *scope;
	size_t  returns, breaks;
	value_t return_;
	bool    returning, breaking, continuing;

	int argc;
	const char **argv;
} env_t;

typedef value_t (*builtin_func_t)(env_t*, expr_t *expr);

typedef struct {
	const char    *name;
	builtin_func_t func;
} builtin_t;

void env_init(  env_t *e, int argc, const char **argv);
void env_deinit(env_t *e);

void eval(env_t *e, stmt_t *program);

#endif
