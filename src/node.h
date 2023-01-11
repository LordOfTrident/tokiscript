#ifndef NODE_H__HEADER_GUARD__
#define NODE_H__HEADER_GUARD__

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */
#include <assert.h> /* static_assert */

#include "common.h"
#include "token.h"

typedef struct expr        expr_t;
typedef struct expr_call   expr_call_t;
typedef struct expr_bin_op expr_bin_op_t;

typedef struct stmt stmt_t;

typedef enum {
	EXPR_TYPE_STR = 0,
	EXPR_TYPE_NUM,
	EXPR_TYPE_BOOL,
	EXPR_TYPE_CALL,
	EXPR_TYPE_BIN_OP,

	EXPR_TYPE_COUNT,
} expr_type_t;

#define CALL_ARGS_CAPACITY 32

struct expr_call {
	char *name;

	expr_t *args[32];
	size_t  args_count;
};

struct expr_bin_op {
	token_type_t type;
	expr_t      *left, *right;
};

struct expr {
	where_t     where;
	expr_type_t type;

	union {
		double        num;
		char         *str;
		bool          bool_;
		expr_call_t   call;
		expr_bin_op_t bin_op;
	} as;
};

static_assert(EXPR_TYPE_COUNT == 5); /* Add new expressions to union */

typedef enum {
	STMT_TYPE_EXPR = 0,

	STMT_TYPE_COUNT,
} stmt_type_t;

struct stmt {
	where_t     where;
	stmt_type_t type;

	union {
		expr_t *expr;
	} as;

	stmt_t *next;
};

static_assert(STMT_TYPE_COUNT == 1); /* Add new statements to union */

expr_t *expr_new(void);
void    expr_free(expr_t *p_expr);

stmt_t *stmt_new(void);
void    stmt_free(stmt_t *p_stmt);

#endif
