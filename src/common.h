#ifndef COMMON_H__HEADER_GUARD__
#define COMMON_H__HEADER_GUARD__

#include <assert.h>  /* assert */
#include <stdlib.h>  /* malloc, exit, EXIT_FAILURE */
#include <string.h>  /* strcpy, strlen */
#include <stdio.h>   /* snprintf */
#include <stdbool.h> /* bool, true, false */

#define UNREACHABLE(__P_MSG) assert(0 && "Unreachable: "__P_MSG)

char *strcpy_to_heap(const char *p_str);
char *shift(int *p_argc, char ***p_argv);

void double_to_str(double p_num, char *p_buf, size_t p_size);

#endif
