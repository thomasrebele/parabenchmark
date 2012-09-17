/*
 * mpi_benchmark.h
 *
 * Measure the bandwidth and execution time of MPI_Send/Recv,
 * MPI_Bcast, MPI_Reduce and execution time of MPI_Barrier
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

#ifndef __MPI_BENCHMARK_H
#define __MPI_BENCHMARK_H

#include "definitions.h"
#include "statistics.h"
#include "config.h"
#include <mpi.h>

typedef struct {
	double bandwidth;
	double time;

} result_network_test_;


struct mpi_test_t_;
typedef struct mpi_test_t_ mpi_test_t;

typedef struct mpi_test_t_ {
	unsigned_huge processes;
	unsigned_huge data_size;
	unsigned_huge steps;

	void *buffer;
	int array_length;
	MPI_Datatype mpi_type;

	double (*testfn)(mpi_test_t*);
} mpi_test_t;

typedef struct mpi_test_t_ mpi_test_t;
#define MPI_TEST_T_INIT {0, 0, 0, NULL, 0, MPI_CHAR, NULL}

typedef struct {
	char *name;
	bool uses_data_size;
	unsigned_huge processes_min;
	unsigned_huge processes_max;
	double (*test_function_ptr)(mpi_test_t*);
} mpi_option_info_t;


STRUCT_ACCESS_ARG_HEADER(result_network_test_, bandwidth, statistics_arg_network_bandwidth)
STRUCT_ACCESS_ARG_HEADER(result_network_test_, time, statistics_arg_network_time)

void start_mpi_bandwidth_benchmark(char *option);
int network_bandwidth_test(mpi_test_t);


#endif
