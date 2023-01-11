#include "common.h"

char *strcpy_to_heap(const char *p_str) {
	char  *str = (char*)malloc(strlen(p_str) + 1);
	if (str == NULL)
		UNREACHABLE("malloc() fail");

	strcpy(str, p_str);

	return str;
}

char *shift(int *p_argc, char ***p_argv) {
	char *arg = **p_argv;

	-- (*p_argc);
	++ (*p_argv);

	return arg;
}

/* None of the printf formats are ideal, because they either
   leave trailing zeros or use scientific format */
void double_to_str(double p_num, char *p_buf, size_t p_size) {
	snprintf(p_buf, p_size, "%f", p_num);

	bool   found = false;
	size_t i;
	for (i = 0; p_buf[i] != '\0'; ++ i) {
		if (p_buf[i] == '.' && !found)
			found = true;
	}

	if (!found)
		return;


	for (-- i;; -- i) {
		if (p_buf[i] != '0') {
			if (p_buf[i] == '.')
				p_buf[i] = '\0';

			break;
		}

		p_buf[i] = '\0';
	}
}
