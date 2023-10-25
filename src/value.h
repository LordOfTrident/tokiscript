#ifndef VALUE_H_HEADER_GUARD
#define VALUE_H_HEADER_GUARD

#include <stdbool.h> /* bool, true, false */
#include <stdlib.h>  /* free */
#include <assert.h>  /* static_assert */

#include "common.h"

typedef enum {
	VALUE_TYPE_NIL = 0,
	VALUE_TYPE_NUM,
	VALUE_TYPE_STR,
	VALUE_TYPE_BOOL,
	VALUE_TYPE_FUN,

	VALUE_TYPE_COUNT,
} value_type_t;

const char *value_type_to_cstr(value_type_t type);

typedef struct {
	value_type_t type;

	union {
		double num;
		char  *str;
		bool   bool_;
		void  *fun;
	} as;
} value_t;

static_assert(VALUE_TYPE_COUNT == 5); /* Add new values to union */

value_t value_nil(void);

value_t value_num( double val);
value_t value_bool(bool   val);
value_t value_str( char  *val);
value_t value_fun( void  *val);

void value_free(value_t *val);

#endif
