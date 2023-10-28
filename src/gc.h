#ifndef GC_H_HEADER_GUARD
#define GC_H_HEADER_GUARD

#include <stdio.h>  /* printf, putchar */
#include <string.h> /* strcmp, strlen, memset */
#include <assert.h> /* static_assert */
#include <math.h>   /* pow */
#include <time.h>   /* time */

/* Welcome to gc.h
 * You should probably stay in the header files since you dont wanna see what the hell is going
 * on under the hood in gc.c and eval.c
 */

#include "value.h"

typedef struct gc_elem {
	value_t val;
	bool    marked;

	struct gc_elem *next;
} gc_elem_t;

typedef struct {
	gc_elem_t *root;
} gc_t;

/* Mark and sweep */
void gc_mas(gc_t *gc, value_t *refs, size_t size);
value_t gc_add_elem(gc_t *gc, value_t val);

#endif
