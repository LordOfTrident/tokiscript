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
typedef struct expr_un_op  expr_un_op_t;
typedef struct expr_do     expr_do_t;
typedef struct expr_fun    expr_fun_t;
typedef struct expr_idx    expr_idx_t;
typedef struct expr_fmt    expr_fmt_t;
typedef struct expr_arr    expr_arr_t;

typedef struct stmt         stmt_t;
typedef struct stmt_let     stmt_let_t;
typedef struct stmt_enum    stmt_enum_t;
typedef struct stmt_if      stmt_if_t;
typedef struct stmt_while   stmt_while_t;
typedef struct stmt_for     stmt_for_t;
typedef struct stmt_foreach stmt_foreach_t;
typedef struct stmt_return  stmt_return_t;
typedef struct stmt_defer   stmt_defer_t;
typedef struct stmt_fun     stmt_fun_t;

typedef enum {
	EXPR_TYPE_VALUE = 0,
	EXPR_TYPE_CALL,
	EXPR_TYPE_ID,
	EXPR_TYPE_BIN_OP,
	EXPR_TYPE_UN_OP,
	EXPR_TYPE_DO,
	EXPR_TYPE_FUN,
	EXPR_TYPE_IDX,
	EXPR_TYPE_FMT,
	EXPR_TYPE_ARR,

	EXPR_TYPE_COUNT,
} expr_type_t;

#define ARGS_CAPACITY 32

struct expr_call {
	expr_t *expr;
	expr_t *args[ARGS_CAPACITY];
	size_t  args_count;
};

struct expr_id {
	char *name;
};

typedef enum {
	BIN_OP_ADD = 0,
	BIN_OP_SUB,
	BIN_OP_MUL,
	BIN_OP_DIV,
	BIN_OP_POW,
	BIN_OP_MOD,

	BIN_OP_ASSIGN,
	BIN_OP_INC,
	BIN_OP_DEC,
	BIN_OP_XINC,
	BIN_OP_XDEC,

	BIN_OP_EQUALS,
	BIN_OP_NOT_EQUALS,
	BIN_OP_GREATER,
	BIN_OP_GREATER_EQU,
	BIN_OP_LESS,
	BIN_OP_LESS_EQU,

	BIN_OP_AND,
	BIN_OP_OR,
	BIN_OP_IN,

	BIN_OP_TYPE_COUNT,
} bin_op_type_t;

struct expr_bin_op {
	bin_op_type_t type;

	expr_t *left, *right;
};

typedef enum {
	UN_OP_POS = 0,
	UN_OP_NEG,
	UN_OP_NOT,

	UN_OP_TYPE_COUNT,
} un_op_type_t;

struct expr_un_op {
	un_op_type_t type;

	expr_t *expr;
};

struct expr_do {
	stmt_t *body;
};

struct expr_fun {
	char   *args[ARGS_CAPACITY];
	size_t  args_count;
	stmt_t *body;
};

struct expr_idx {
	expr_t *expr, *start, *end;
};

struct expr_fmt {
	char   *str;
	expr_t *args[ARGS_CAPACITY];
	size_t  args_count;
};

struct expr_arr {
	expr_t **buf;
	size_t   size, cap;
};

struct expr {
	where_t     where;
	expr_type_t type;

	union {
		value_t       val;
		expr_call_t   call;
		expr_id_t     id;
		expr_bin_op_t bin_op;
		expr_un_op_t  un_op;
		expr_do_t     do_;
		expr_fun_t    fun;
		expr_idx_t    idx;
		expr_fmt_t    fmt;
		expr_arr_t    arr;
	} as;
};

static_assert(EXPR_TYPE_COUNT == 10); /* Add new expressions to union */

typedef enum {
	STMT_TYPE_EXPR = 0,
	STMT_TYPE_LET,
	STMT_TYPE_ENUM,
	STMT_TYPE_IF,
	STMT_TYPE_WHILE,
	STMT_TYPE_FOR,
	STMT_TYPE_FOREACH,
	STMT_TYPE_RETURN,
	STMT_TYPE_DEFER,
	STMT_TYPE_BREAK,
	STMT_TYPE_CONTINUE,
	STMT_TYPE_FUN,

	STMT_TYPE_COUNT,
} stmt_type_t;

struct stmt_let {
	char   *name;
	expr_t *val;
	stmt_t *next;
	bool    const_;
};

struct stmt_if {
	expr_t *cond;
	stmt_t *body, *else_, *next;
};

struct stmt_while {
	expr_t *cond;
	stmt_t *body;
};

struct stmt_for {
	expr_t *cond;
	stmt_t *init, *step;
	stmt_t *body;
};

struct stmt_foreach {
	char   *name, *it;
	expr_t *in;
	stmt_t *body;
};

struct stmt_return {
	expr_t *expr;
};

struct stmt_defer {
	stmt_t *stmt;
};

struct stmt_fun {
	char   *name;
	expr_t *def;
};

struct stmt_enum {
	char   *name;
	stmt_t *next;
};

struct stmt {
	where_t     where;
	stmt_type_t type;

	union {
		expr_t        *expr;
		stmt_let_t     let;
		stmt_if_t      if_;
		stmt_while_t   while_;
		stmt_for_t     for_;
		stmt_foreach_t foreach;
		stmt_return_t  return_;
		stmt_defer_t   defer;
		stmt_fun_t     fun;
		stmt_enum_t    enum_;
	} as;

	stmt_t *next;
};

static_assert(STMT_TYPE_COUNT == 12); /* Add new statements to union */

expr_t *expr_new(void);
void    expr_free(expr_t *expr);

stmt_t *stmt_new(void);
void    stmt_free(stmt_t *stmt);

#endif
