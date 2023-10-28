#include "gc.h"

/*
 *  --- ABANDON ALL HOPE, YE WHO ENTER HERE ---
 *      This is some of the ugliest code
 *      ive ever put on Github. This
 *      language is just one big experiment
 *      toy that i wanted to try stuff on
 *      and see how it goes. I did not
 *      spend any time cleaning anything up
 *      or refactoring anything. I just
 *      piled more shit onto the shit that
 *      already was there, and glued it all
 *      together. I tried stuff and i found
 *      out what does and doesnt work, so
 *      next time i can do things corectly.
 *
 *      You have been warned.
 *  --- ----------------------------------- ---
 */

static void gc_mark_array(gc_t *gc, value_t val);

/* I know these recursive loops are extremely slow and i dont care currently.
   If python devs dont care about speed, why should i. */
static void gc_find_and_mark(gc_t *gc, value_t val) {
	for (gc_elem_t *elem = gc->root; elem != NULL; elem = elem->next) {
		if (elem->val.type != val.type)
			continue;

		if (elem->val.type == VALUE_TYPE_STR) {
			if (elem->val.as.str == val.as.str) {
				elem->marked = true;
				break;
			}
		} else if (elem->val.type == VALUE_TYPE_ARR) {
			if (elem->val.as.arr.buf == val.as.arr.buf) {
				elem->marked = true;
				gc_mark_array(gc, val);
				break;
			}
		}
	}
}

static void gc_mark_array(gc_t *gc, value_t val) {
	for (size_t i = 0; i < val.as.arr.size; ++ i) {
		if (val.as.arr.buf[i].type == VALUE_TYPE_STR || val.as.arr.buf[i].type == VALUE_TYPE_ARR)
			gc_find_and_mark(gc, val.as.arr.buf[i]);
	}
}

static void gc_find_and_mark_in_array(gc_t *gc, gc_elem_t *elem, value_t val) {
	UNUSED(gc);
	for (size_t i = 0; i < val.as.arr.size; ++ i) {
		if (val.as.arr.buf[i].type != elem->val.type)
			continue;

		if (elem->val.type == VALUE_TYPE_STR) {
			if (elem->val.as.str == val.as.arr.buf[i].as.str) {
				elem->marked = true;
				break;
			}
		} else if (elem->val.type == VALUE_TYPE_ARR) {
			if (elem->val.as.arr.buf == val.as.arr.buf[i].as.arr.buf) {
				elem->marked = true;
				break;
			}
		}
	}
}

static void gc_mark(gc_t *gc, gc_elem_t *elem, value_t *refs, size_t size) {
	elem->marked = false;
	for (size_t i = 0; i < size; ++ i) {
		if (elem->val.type != refs[i].type)
			continue;

		if (elem->val.type == VALUE_TYPE_STR) {
			if (elem->val.as.str == refs[i].as.str) {
				elem->marked = true;
				break;
			}
		} else if (elem->val.type == VALUE_TYPE_ARR) {
			if (elem->val.as.arr.buf == refs[i].as.arr.buf) {
				elem->marked = true;
				break;
			}
		}
	}

	if (!elem->marked) {
	for (size_t i = 0; i < size; ++ i) {
			if (refs[i].type == VALUE_TYPE_ARR)
				gc_find_and_mark_in_array(gc, elem, refs[i]);
		}
	}

	if (elem->marked && elem->val.type == VALUE_TYPE_ARR)
		gc_mark_array(gc, elem->val);
}

void gc_mas(gc_t *gc, value_t *refs, size_t size) {
	for (gc_elem_t *elem = gc->root; elem != NULL; elem = elem->next)
		gc_mark(gc, elem, refs, size);

	gc_elem_t **prev_next = &gc->root, *elem = gc->root;
	while (elem != NULL) {
		if (!elem->marked) {
			value_free(&elem->val);

			gc_elem_t *next = elem->next;
			free(elem);
			elem = next;
			*prev_next = elem;
		} else {
			prev_next = &elem->next;
			elem = elem->next;
		}
	}
}

value_t gc_add_elem(gc_t *gc, value_t val) {
	gc_elem_t *elem = (gc_elem_t*)malloc(sizeof(gc_elem_t));
	if (elem == NULL)
		UNREACHABLE("malloc() fail");

	elem->marked = false;
	elem->val    = val;
	elem->next   = gc->root;
	gc->root     = elem;
	return val;
}
