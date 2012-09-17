#ifndef __PTHREAD_BENCHMARK_H
#define __PTHREAD_BENCHMARK_H

#include "definitions.h"
#include "statistics.h"

typedef struct pthread_create_benchmark_result_t_ {
	unsigned num_threads;
	double total_time;
	double single_time;
	double startup_time;
} pthread_create_benchmark_result_t;

STRUCT_ACCESS_ARG_HEADER(pthread_create_benchmark_result_t_, total_time, statistics_arg_pthread_create_total_time)
STRUCT_ACCESS_ARG_HEADER(pthread_create_benchmark_result_t_, single_time, statistics_arg_pthread_create_single_time)
STRUCT_ACCESS_ARG_HEADER(pthread_create_benchmark_result_t_, startup_time, statistics_arg_pthread_create_startup_time)

void* simple_integer_arithmetic_loop(void *arg);
void* simple_float_arithmetic_loop(void *arg);

void start_pthread_loop_benchmark();
void pthread_loop_test(
		unsigned num_threads,
		unsigned_huge iterations,
		bool reduce,
		void *(*loop_function_ptr)(void *));
unsigned pthread_loop_calculate_repetitions(
				unsigned_huge iterations,
				void *(*loop_function_ptr)(void *));

void start_pthread_create_benchmark();
void pthread_create_benchmark(unsigned num_threads, int detachstate, int stacksize_faktor);

#endif
