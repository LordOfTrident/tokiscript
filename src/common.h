#ifndef COMMON_H__HEADER_GUARD__
#define COMMON_H__HEADER_GUARD__

#include <assert.h>  /* assert */
#include <stdlib.h>  /* malloc, exit, EXIT_FAILURE */
#include <string.h>  /* strcpy, strlen */
#include <stdio.h>   /* snprintf */
#include <stdbool.h> /* bool, true, false */

#define UNREACHABLE(MSG) assert(0 && "Unreachable: "MSG)

char *strcpy_to_heap(const char *str);
char *shift(int *argc, char ***argv);

void double_to_str(double num, char *buf, size_t size);

#endif
