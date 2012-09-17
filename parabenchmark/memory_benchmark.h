/*
 * memory_benchmark.h
 *
 * Attept to measure the memory bandwidth. Probably don't return valid values.
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

#ifndef __MEMORY_BENCHMARK_H
#define __MEMORY_BENCHMARK_H

#include "definitions.h"

typedef struct {
	unsigned_huge steps;
	unsigned_huge data_size;
	unsigned_huge blocksize;
	long stride;

	volatile void *buffer;
	bool uses_stride;
} memory_function_arg_t;

typedef struct {
	double time;
	double overhead;
	double transmitted;

	unsigned overhead_context_switches;
	unsigned time_context_switches;

	bool datasize_enough;

	unsigned_huge blocksize;
	unsigned_huge stride;
} memory_result_t;

#define MEMORY_RESULT_T_INIT {0.0, 0.0, 0.0, false}

typedef memory_result_t (*access_fn_t)(memory_function_arg_t arg);

typedef struct {
	char *name;
	access_fn_t access_fn;
	bool uses_stride;
} memory_option_info_t;

void start_memory_bandwidth_benchmark(char *option);

unsigned_huge max_malloc_arg();

#endif
