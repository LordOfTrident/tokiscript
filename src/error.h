#ifndef ERROR_H_HEADER_GUARD
#define ERROR_H_HEADER_GUARD

#include <stdarg.h> /* va_list, va_start, va_end, vsnprintf */
#include <stdio.h>  /* stderr, fprintf */

#include <chol/colorer.h>

#include "token.h"
#include "value.h"

typedef struct {
	where_t where;
	char   *name;
} call_t;

extern call_t *callstack;
extern size_t *callstack_size;

void from(           where_t where);
void error(          where_t where, const char *fmt, ...);
void wrong_type(     where_t where, value_type_t type, const char *in);
void wrong_arg_count(where_t where, size_t got, size_t expected);
void undefined(      where_t where, const char *name);
void print_callstack(void);

#endif
