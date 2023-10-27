#include "value.h"

static const char *value_type_to_cstr_map[] = {
	[VALUE_TYPE_NIL]  = "nil",
	[VALUE_TYPE_STR]  = "string",
	[VALUE_TYPE_NUM]  = "number",
	[VALUE_TYPE_BOOL] = "boolean",
	[VALUE_TYPE_FUN]  = "function",
	[VALUE_TYPE_NAT]  = "native",
	[VALUE_TYPE_ARR]  = "array",
};

static_assert(VALUE_TYPE_COUNT == 7); /* Add the new value type to the map */

const char *value_type_to_cstr(value_type_t type) {
	if (type >= VALUE_TYPE_COUNT)
		UNREACHABLE("Invalid value type");

	return value_type_to_cstr_map[type];
}

value_t value_nil(void) {
	return (value_t){.type = VALUE_TYPE_NIL};
}

value_t value_num(double val) {
	return (value_t){.type = VALUE_TYPE_NUM, .as = {.num = val}};
}

value_t value_bool(bool val) {
	return (value_t){.type = VALUE_TYPE_BOOL, .as = {.bool_ = val}};
}

value_t value_str(char *val) {
	return (value_t){.type = VALUE_TYPE_STR, .as = {.str = val}};
}

value_t value_fun(void *val) {
	return (value_t){.type = VALUE_TYPE_FUN, .as = {.fun = val}};
}

value_t value_nat(value_t (*val)()) {
	return (value_t){.type = VALUE_TYPE_NAT, .as = {.nat = val}};
}

value_t value_arr(size_t size) {
	value_t val = {0};
	val.type = VALUE_TYPE_ARR;

	val.as.arr.cap  = ARRAY_CHUNK_SIZE;
	val.as.arr.size = size;
	while (val.as.arr.cap < size)
		val.as.arr.cap *= 2;

	val.as.arr.buf = (value_t*)malloc(val.as.arr.cap * sizeof(value_t));
	if (val.as.arr.buf == NULL)
		UNREACHABLE("malloc() fail");

	return val;
}

void value_free(value_t *val) {
	if (val->type == VALUE_TYPE_STR)
		free(val->as.str);
	else if (val->type == VALUE_TYPE_ARR)
		free(val->as.arr.buf);
}
