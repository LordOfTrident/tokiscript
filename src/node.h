#ifndef NODE_H_HEADER_GUARD
#define NODE_H_HEADER_GUARD

#include <stdlib.h> /* malloc, free */
#include <string.h> /* memset */
#include <assert.h> /* static_assert */

#include "common.h"
#include "token.h"
#include "value.h"

typedef struct expr        expr_t;
typedef struct expr_call   expr_call_t;
typedef struct expr_id     expr_id_t;
typedef struct expr_bin_op expr_bin_op_t;

typedef struct stmt     stmt_t;
typedef struct stmt_let stmt_let_t;

typedef enum {
	EXPR_TYPE_VALUE = 0,
	EXPR_TYPE_CALL,
	EXPR_TYPE_ID,
	EXPR_TYPE_BIN_OP,

	EXPR_TYPE_COUNT,
} expr_type_t;

#define CALL_ARGS_CAPACITY 32

struct expr_call {
	char *name;

	expr_t *args[32];
	size_t  args_count;
};

struct expr_id {
	char *name;
};

struct expr_bin_op {
	token_type_t type;
	expr_t      *left, *right;
};

struct expr {
	where_t     where;
	expr_type_t type;

	union {
		value_t       val;
		expr_call_t   call;
		expr_id_t     id;
		expr_bin_op_t bin_op;
	} as;
};

static_assert(EXPR_TYPE_COUNT == 4); /* Add new expressions to union */

typedef enum {
	STMT_TYPE_EXPR = 0,
	STMT_TYPE_LET,

	STMT_TYPE_COUNT,
} stmt_type_t;

typedef struct stmt_let {
	char   *name;
	expr_t *val;
} stmt_let_t;

struct stmt {
	where_t     where;
	stmt_type_t type;

	union {
		expr_t    *expr;
		stmt_let_t let;
	} as;

	stmt_t *next;
};

static_assert(STMT_TYPE_COUNT == 2); /* Add new statements to union */

expr_t *expr_new(void);
void    expr_free(expr_t *expr);

stmt_t *stmt_new(void);
void    stmt_free(stmt_t *stmt);

#endif
