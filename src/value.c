#include "value.h"

static const char *value_type_to_cstr_map[] = {
	[VALUE_TYPE_NIL]  = "nil",
	[VALUE_TYPE_STR]  = "string",
	[VALUE_TYPE_NUM]  = "number",
	[VALUE_TYPE_BOOL] = "boolean",
};

static_assert(VALUE_TYPE_COUNT == 4); /* Add the new value type to the map */

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

void value_free(value_t *val) {
	if (val->type == VALUE_TYPE_STR)
		free(val->as.str);
}
