/*
 * nested_for.h
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

#ifndef __NESTED_FOR_H
#define __NESTED_FOR_H

#include "definitions.h"
#include "range.h"
#include <stdbool.h>
#include <stdlib.h>

typedef struct {
	char *name;
	unsigned_huge start;
	unsigned_huge end;
	unsigned_huge i;
	range_t *range;
} iteration_var_t;

struct for_loop_t_;
typedef struct for_loop_t_ for_loop_t;

unsigned_huge step_increment(for_loop_t *loop);
unsigned_huge step_exponential(for_loop_t *loop);
unsigned_huge step_range(for_loop_t *loop);

struct for_loop_t_ {
	iteration_var_t var;
	for_loop_t *next;

	int (*outer_start_fn)(unsigned level, iteration_var_t *i);
	int (*outer_end_fn)(unsigned level, iteration_var_t *i);
	int (*inner_start_fn)(unsigned level, iteration_var_t *i);
	int (*inner_end_fn)(unsigned level, iteration_var_t *i);

	unsigned_huge (*step_fn)(for_loop_t*);
};

#define FOR_LOOP_T_INIT { \
	{"", 0, 0, 0, NULL}, \
	NULL, NULL, NULL, NULL, NULL, \
	&step_increment}

#define NESTED_FOR_CONT -1
#define NESTED_FOR_BREAK 1

int _nested_for_loop(for_loop_t *loops,
		unsigned level,
		iteration_var_t *i,
		int (*innermost_fn)(unsigned level, iteration_var_t *i));

void nested_for_loop(for_loop_t *loops,
		int (*innermost_fn)(unsigned level, iteration_var_t *i));

int get_iteration_value(char *name,
		unsigned level, iteration_var_t *vec, unsigned_huge *value);

void print_iteration_vec(unsigned level,
		iteration_var_t *vec);

#endif
