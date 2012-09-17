/*
 * nested_for.c
 *
 * Nested for loops as a linked list of structs. Allows to change the
 * loop order dynamically.
 *
 * Copyright (C) 2012  Thomas Rebele (thomas.rebele at mytum dot de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "nested_for.h"
#include "range.h"

/**
 * calculate next step incremental. Obsolete, 'range' is recommended
 */
unsigned_huge step_increment(for_loop_t *loop) {
	unsigned_huge j = loop->var.i;
	return j+1;
}

/**
 * calculate next step exponential. Obsolete, 'range' is recommanded
 */
unsigned_huge step_exponential(for_loop_t *loop) {
	unsigned_huge j = loop->var.i;
	if(j==0)
		return 1;
	else
		return 2*j;
}

/**
 * dummy function
 */
unsigned_huge step_range(for_loop_t *loop) {
	// dummy
	return -1;
}

/**
 * Get value of iteration variable by name
 */
int get_iteration_value(char *name,
		unsigned level, iteration_var_t *vec, unsigned_huge *value) {
	int i; bool found = false;
	for(i=0; i<level; i++) {
		if(strcmp(name, vec[i].name) == 0) {
			found = true;
			*value = vec[i].i;
			break;
		}
	}
	if(found)
		return 0;
	else
		return 1;
}

/**
 * Print iteration vector for debugging
 */
void print_iteration_vec(unsigned level,
		iteration_var_t *vec) {
	int i;
	for(i=0; i<level; i++) {
		printf("%s: %Lu, ", vec[i].name, vec[i].i);
	}
	printf("\n");
}

/**
 * Execute nested for loop recursively
 */
int _nested_for_loop(for_loop_t *loops,
		unsigned level,
		iteration_var_t *vec,
		int (*innermost_fn)(unsigned, iteration_var_t *)) {

	if(loops == NULL) {
		if(innermost_fn != NULL)
			innermost_fn(level, vec);
	}
	else {
		vec[level].name = loops->var.name;
		if(loops->outer_start_fn != NULL) {
			int val = loops->outer_start_fn(level, vec);
		}
		level++;
		unsigned_huge i;
		if(loops->step_fn == &step_range) {
			range_reset(loops->var.range);
			while(range_next(loops->var.range, &i)) {
/* MAKRO START */
#define NESTED_FOR_INNER_LOOP \
				loops->var.i = i; \
				vec[level-1].i = i; \
				if(loops->inner_start_fn != NULL) { \
					int val = loops->inner_start_fn(level, vec); \
					if(val == NESTED_FOR_BREAK) break; \
					else if(val == NESTED_FOR_CONT) continue; \
				} \
				_nested_for_loop(loops->next, level, vec, innermost_fn); \
				if(loops->inner_end_fn != NULL) { \
					int val = loops->inner_end_fn(level, vec); \
					if(val == NESTED_FOR_BREAK) break; \
				}
/* MAKRO END */
				NESTED_FOR_INNER_LOOP
			}
		}
		else {
			for(i=loops->var.start; i<loops->var.end; i=loops->step_fn(loops)) {
				NESTED_FOR_INNER_LOOP
			}
		}
		level--;
		if(loops->outer_end_fn != NULL) {
			int val = loops->outer_end_fn(level, vec);
		}
	}
	return 0;
}

/**
 * start nested for loop
 */
void nested_for_loop(for_loop_t *loops,
		int (*innermost_fn)(unsigned level, iteration_var_t *i)) {
	int num_loops = 0;
	for_loop_t *ptr = loops;
	while(ptr != NULL) {
		num_loops++;
		ptr = ptr->next;
	}
	num_loops++;
	iteration_var_t *vec = (iteration_var_t*) malloc(num_loops*sizeof(iteration_var_t));
	vec->name = "nested_for_loop_levels"; vec->i = num_loops-1;
	_nested_for_loop(loops, 0, vec, innermost_fn);

	free(vec);
}
