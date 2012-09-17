/*
 * range.c
 *
 * Data structure and functions for generating incremental or exponential
 * sequences of integers.
 *
 * examples: start-end,start-end
 * with options + addition, * factors
 *
 *    3-20          3,6,12
 *    3-5[]         3,4,5
 *    3-33[+10]     3,13,23,33
 *    1-1000[*10]   1,10,100,1000
 *    8-64[*1.4142135623*2]   8,11,16,22,32,45,64
 *    10-1000[*2.5*5*7.5*10]  10,25,50,75,100,250,500,750,1000
 *    10-40,2-5[]   10,20,40,2,3,4,5
 *    1-4[],2-5[]   1,2,3,4,2,3,4,5
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

#include "range.h"

#define INTERN
#ifdef EXTERN
#undef INTERN
#endif

/**
 * reset range, to generate next number from the begin
 */
void range_reset(range_t *range) {
	range->_next = 0;
	range->factors._index = 0;
	if(range->next_range != NULL) {
		range_reset(range->next_range);
	}
}

/**
 * free occupied memory of range
 */
void range_free(range_t *range) {
	if(range->next_range != NULL) {
		range_free(range->next_range);

	}
	free(range);
}

/**
 * generate next number for range and store it in 'next'
 * return 1 if successful
 */
int range_next(range_t *range, unsigned_huge *next) {
	if(range == NULL || next == NULL) return 0;
	if(range->_next < range->start) {
		range->_next = range->start;
	}
	if(range->_next > range->end) {
		if(range->next_range == NULL)
			return 0;
		else
			return range_next(range->next_range, next);
	}
	*next = range->_next;

	if(range->factors.size == 0) {
		range->_next += range->addition;
	}
	else {
		if(range->_next == 0
				|| range->factors.values == NULL
				|| range->factors.size == 0) {
			range->_next = range->addition;
			return 1;
		}
		else {
			int i;
			for(i=0; i<range->factors.size; i++) {
				unsigned factor_idx;
				factor_idx = range->factors._index % range->factors.size;

				if(factor_idx == 0) {
					range->_before_factor = range->_next;
				}
				double factor = range->factors.values[factor_idx];
				range->factors._index = factor_idx+1;

				range->_next = range->_before_factor * factor;
				range->_next += range->factors.after_addition;

				if(*next != range->_next) break;
			}
			if(*next == range->_next) {
				range->_next += range->addition;
			}
		}
	}
	return 1;
}

/**
 * add factor to range
 */
void range_add_factor(range_t *range, double factor) {
	unsigned newsize = range->factors.size + 1;
	double *newvalues = (double*)realloc(range->factors.values, newsize * sizeof(double));
	if(newvalues != NULL) {
		range->factors.values = newvalues;
		newvalues[range->factors.size++] = factor;
	}
}

/**
 * print intern informations for debugging recursively
 */
void range_print_intern(char *prefix, range_t *range, unsigned level) {
	if(range == NULL) return;
	if(prefix == NULL) prefix = "";
	_printf("%s", prefix);
	if(level != 0) _printf("+ ");
	_printf("range start=%Lu, end=%Lu, ", range->start, range->end);
	_printf("addition=%Lu;\n", range->addition);
	int i;
	if(range->factors.values != NULL) {
		_printf("%s\tfactors=", prefix);
		for(i=0; i<range->factors.size; i++) {
			if(i!=0) _printf(" ");
			_printf("%f", range->factors.values[i]);
		}
		_printf(", after_addition=%Lu;\n", range->factors.after_addition);
	}

	range_print_intern(prefix, range->next_range, level+1);
}

/**
 * print intern informations for debugging
 */
void range_print(char *prefix, range_t *range) {
	range_print_intern(prefix,range,0);
}
