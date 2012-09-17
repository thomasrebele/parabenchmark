/*
 * range.h
 *
 * Data structure and functions for generating incremental or exponential
 * sequences of integers.
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

#ifndef __RANGE_H
#define __RANGE_H

#include "definitions.h"

struct range_t_;
typedef struct range_t_ range_t;
struct range_t_ {
	unsigned_huge start;
	unsigned_huge end;

	struct {
		double *values;
		unsigned size;
		unsigned_huge after_addition;
		unsigned _index;
	} factors;

	unsigned_huge addition;

	unsigned_huge _next;
	unsigned_huge _before_factor;

	range_t *next_range;
};
#define RANGE_T_INIT {1,-2, {NULL, 0, 0, 0}, 1, 0, 0, NULL}

void range_print(char *prefix, range_t *range);
void range_reset(range_t *range);
int range_next(range_t *range, unsigned_huge *next);
void range_add_factor(range_t *range, double factor);
void range_free(range_t *range);
#endif
