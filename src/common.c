#include "common.h"

char *strcpy_to_heap(const char *str) {
	char  *copy = (char*)malloc(strlen(str) + 1);
	if (copy == NULL)
		UNREACHABLE("malloc() fail");

	strcpy(copy, str);

	return copy;
}

char *shift(int *argc, char ***argv) {
	char *arg = **argv;

	-- (*argc);
	++ (*argv);

	return arg;
}

/* None of the printf formats are ideal, because they either
   leave trailing zeros or use scientific format */
void double_to_str(double num, char *buf, size_t size) {
	snprintf(buf, size, "%f", num);

	bool   found = false;
	size_t i;
	for (i = 0; buf[i] != '\0'; ++ i) {
		if (buf[i] == '.' && !found)
			found = true;
	}

	if (!found)
		return;


	for (-- i;; -- i) {
		if (buf[i] != '0') {
			if (buf[i] == '.')
				buf[i] = '\0';

			break;
		}

		buf[i] = '\0';
	}
}

char *readfile(const char *path) {
	FILE *file = fopen(path, "r");
	if (file == NULL)
		return NULL;

	fseek(file, 0, SEEK_END);
	size_t size = (size_t)ftell(file);
	rewind(file);

	char *str = (char*)malloc(size + 1);
	if (str == NULL)
		UNREACHABLE("malloc() fail");

	if (fread(str, size, 1, file) <= 0)
		return NULL;

	str[size] = '\0';
	fclose(file);
	return str;
}
