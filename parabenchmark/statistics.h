/*
 * statistics.h
 *
 * Calculate the mean and deviation
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
#ifndef __STATISTICS_H
#define __STATISTICS_H

#include "definitions.h"
#include <math.h>

typedef struct statistic_t_ {
	double mean;
	double deviation; // standard deviation
	double error; // standard error
	unsigned_huge sample_size;

	// for intern usage only
	double _m;
	double _q;
} statistic_t;

#define STATISTIC_T_INIT {NAN,NAN,NAN,0,0,0}

void calculate_statistics_iterative(statistic_t *stat, double value);

statistic_t calculate_statistics(
		double (*f)(void *, void *, int),
		void *f_arg,
		void *start,
		int length);

double median(double *array, int size);
statistic_t middle_stat(double *array, int size);

typedef struct {
	//char* description;
	size_t length;
	size_t offset;
} struct_access_args_t;

double struct_access(void *args_void, void *start_void, int index);
double direct_double_access(void *args_void, void *start_void, int index);

#define STRUCT_ACCESS_ARG_HEADER(structname, field, typename) \
	struct_access_args_t typename ## _; \
	void *typename;

#define STRUCT_ACCESS_ARG(structname, field, typename) \
	struct_access_args_t typename ## _ = { \
			sizeof(structname), \
			offsetof(structname, field) \
	}; \
	void *typename = (void *) & typename ## _;

#endif
