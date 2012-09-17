/*
 * range.h
 *
 * Benchmark speedup with MPI and Pthreads
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

#include <pthread.h>
#include <limits.h>
#include <unistd.h>
#ifdef COMPILE_WITH_MPI
#include <mpi.h>
#endif

#include "speedup_benchmark.h"
#include "definitions.h"
#include "config.h"
#include "timer.h"
#include "statistics.h"
#include "print_functions.h"
#include "getopt.h"
#include "nested_for.h"
#include "parse.h"

#include "pthread_functions.h"
#include "pthread_benchmark.h"

extern config_t config;
#ifdef COMPILE_WITH_MPI
#include "mpi_functions.h"
extern int world_size;
extern int world_rank;
#endif


/**
 * data structure to save results for faster processing
 */
typedef struct {
	unsigned_huge iterations;
	double *measurement_time;
	unsigned_huge size;
} serial_time_cache_t;

serial_time_cache_t SERIAL_TIME_CACHE_T_INIT = {0, NULL, 0};

/**
 * Get cache line for data size 'iterations' (warning: name mismatch)
 */
serial_time_cache_t *get_cache_line(
		unsigned *size_ptr,
		serial_time_cache_t **cache_ptr,
		unsigned_huge iterations)
{
	unsigned size = *size_ptr;
	serial_time_cache_t *cache = *cache_ptr;
	int i;
	for(i=0; i<size; i++) {
		if(cache[i].iterations == iterations)
			return &cache[i];
	}
	unsigned newsize = size + 1;
	unsigned long int bytes = (2*newsize+1) * sizeof(serial_time_cache_t);
	void *new_ptr = realloc(cache, bytes);
	if(new_ptr == NULL) return NULL;
	cache = (serial_time_cache_t*)new_ptr;
	cache[size] = SERIAL_TIME_CACHE_T_INIT;
	cache[size].iterations = iterations;

	*cache_ptr = cache;
	*size_ptr = newsize;
	return &cache[size];
}

unsigned speedup_calculate_repetitions(
		unsigned_huge iterations,
		void *(*loop_function_ptr)(void *));

void speedup_benchmark(
		unsigned_huge num_processes,
		unsigned_huge num_threads,
		unsigned_huge iterations,
		bool reduce,
		void *(*loop_function_ptr)(void *),
		serial_time_cache_t *cacheline);

/**
 * parse speedup options and start benchmark
 */
void start_speedup_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("speedup benchmark\n");
	_printf("###############\n");
	void *(*loop_function_ptr)(void *) = &simple_integer_arithmetic_loop;
	bool reduce = false;
	print_table_set_additional_info("reduce option", "noreduce");

	char *use_reduction = "reduce", *no_reduction = "noreduce";
	char *loop_fn_int = "int", *loop_fn_float = "float";

	// generate additional table columns
	char *additional_info_header = "reduce option, loop function";
	char *additional_info_reduce = "noreduce, ", *additional_info = NULL;

	get_thread_array(config.threads->end);

	if(option == NULL) option = "int";
	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
	char *token;
	while((token = get_token(&get_token_pointers, option, NULL)) != NULL) {
		// process pthread / loop option
		if(strcmp(token, use_reduction) == 0) {
			reduce = true;
			additional_info_reduce = "reduce, ";
			continue;
		}
		if(strcmp(token, no_reduction) == 0) {
			reduce = false;
			additional_info_reduce = "noreduce, ";
			continue;
		}

		free(additional_info);
		additional_info = NULL;
		strappend(&additional_info, additional_info_reduce);

		if(strcmp(token, loop_fn_int) == 0) {
			loop_function_ptr = &simple_integer_arithmetic_loop;
			strappend(&additional_info, "int, ");
		}
		else if(strcmp(token, loop_fn_float) == 0) {
			loop_function_ptr = &simple_float_arithmetic_loop;
			strappend(&additional_info, "float, ");

		}
		else {
			_printf("WARNING: unknown option for loop benchmark: %s\n", token);
			continue;
		}

		print_table_set_additional_info(additional_info_header, additional_info);

		unsigned cache_size = 0;
		serial_time_cache_t *cache = NULL;

		// initialized 'nested_for' loops
		for_loop_t process_loop = FOR_LOOP_T_INIT;
		process_loop.var.range = config.processes;
#ifdef COMPILE_WITH_MPI
		process_loop.var.start = config.processes->start;
		if(config.processes->end == 0 || config.processes->end > world_size) {
			process_loop.var.range->end = world_size;
		}
		else {
			process_loop.var.range->end = config.processes->end;
		}
#else
		process_loop.var.range->start = 1;
		process_loop.var.range->end = 1;
#endif
		process_loop.var.name = "process";
		process_loop.step_fn = &step_range;

		for_loop_t thread_loop = FOR_LOOP_T_INIT;
		thread_loop.var.name = "thread";
		thread_loop.var.range = config.threads;
		thread_loop.step_fn = &step_range;

		for_loop_t iteration_loop = FOR_LOOP_T_INIT;
		iteration_loop.var.name = "iteration";
		iteration_loop.var.range = config.range;
		iteration_loop.step_fn = &step_range;

		/*
		 * inline function that starts the benchmarks
		 */
		iteration_loop.next = &process_loop;
		process_loop.next = &thread_loop;
		int fn(unsigned level, iteration_var_t *vec) {
			unsigned_huge num_processes; get_iteration_value("process", level, vec, &num_processes);
			unsigned_huge num_threads; get_iteration_value("thread", level, vec, &num_threads);
			unsigned_huge iterations; get_iteration_value("iteration", level, vec, &iterations);

			serial_time_cache_t *cacheline = get_cache_line(&cache_size, &cache, iterations);
			speedup_benchmark(num_processes, num_threads, iterations,
					reduce, loop_function_ptr, cacheline);
			return 0;
		}

		int print_header_fn(unsigned level, iteration_var_t *vec) {
			print_header();
			return 0;
		}
		if(process_loop.var.range->end - process_loop.var.range->start <= 1) {
			if(thread_loop.var.range->end - thread_loop.var.range->start <= 1) {
				print_header();
			}
			else {
				thread_loop.outer_start_fn = &print_header_fn;
			}
		}
		else {
			process_loop.outer_start_fn = &print_header_fn;
		}
		nested_for_loop(&iteration_loop, fn);
	}
	get_token(&get_token_pointers, NULL, NULL);

	free(additional_info);
}

bool intern_speedup_benchmark = false;
statistic_t intern_speedup_stat = STATISTIC_T_INIT;
statistic_t intern_total_stat = STATISTIC_T_INIT;
statistic_t intern_serial_stat = STATISTIC_T_INIT;
/**
 * speedup benchmark using loop functions from pthread_benchmark.c
 * if num_threads == 1, the loop function is executed in main thread
 */
void speedup_benchmark(
		unsigned_huge num_processes,
		unsigned_huge num_threads,
		unsigned_huge iterations,
		bool reduce,
		void *(*loop_function_ptr)(void *),
		serial_time_cache_t *cacheline) {

	unsigned_huge i;
	statistic_t thread_time_total_stat = STATISTIC_T_INIT;
	statistic_t serial_time_stat = STATISTIC_T_INIT;
	statistic_t speedup_stat = STATISTIC_T_INIT;
	int repetitions = 0;

	if(num_processes * num_threads > iterations ||
			num_threads == 0 ||
			num_processes == 0 ||
			(num_threads == 1 && num_processes == 1)) {

	}
	else {
		repetitions = speedup_calculate_repetitions(iterations, loop_function_ptr);

		thread_arg_t *args = get_thread_array(num_threads);
		if(args == NULL) {
			_printf("Cannot allocate enough space for threads\n");
			return;
		}
		for(i = 0; i<num_threads; i++) {
			args[i].reduce = reduce;
			args[i].thread_count = num_threads;
			args[i].loop_function = loop_function_ptr;
		}

		unsigned_huge thread_iterations;
		unsigned_huge rest;
		#ifdef COMPILE_WITH_MPI
			thread_iterations = iterations / (num_processes*num_threads);
			rest = iterations % (num_threads*num_processes);
			MPI_Comm comm;
		if(create_sub_comm(num_processes, &comm)) {
			int processid;
			MPI_Comm_rank(comm, &processid);
		#else
			num_processes = 1;
			thread_iterations = iterations / num_threads;
			rest = iterations % num_threads;
		#endif
		thread_iterations += (rest == 0) ? 0 : 1;

		thread_arg_t arg = THREAD_ARG_T_INIT;
		arg.iteration_start = 0;
		arg.iteration_end = iterations;
		double *serial_time = NULL; bool run_serial = true;

		if(cacheline != NULL) {
			if(cacheline->size < repetitions) {
				run_serial = true;
				cacheline->measurement_time =
					(double*)realloc(
						cacheline->measurement_time,
						sizeof(double)*repetitions);
				cacheline->size = repetitions;
			}
			else {
				run_serial = false;
			}
			serial_time = cacheline->measurement_time;
		}
		else {
			serial_time = (double *)malloc(sizeof(double)*repetitions);
		}
		if(serial_time == NULL) {
			_printf("WARNING: serial_time is NULL\n");
			return;
		}
		int r;
		for(r=-config.warmup; r<repetitions; r++) {
			// measurement serial time
			double measurement_time = 0;

			if(run_serial) {
				tick(MODE_START);
				loop_function_ptr(&arg);
				measurement_time = tick(MODE_END);
				if(r>=0) {
					serial_time[r] = measurement_time;
					calculate_statistics_iterative(&serial_time_stat, measurement_time);
				}
			}
			else {
				if(r>=0) {
					calculate_statistics_iterative(&serial_time_stat, serial_time[r]);
				}
			}
		}

		for(r=-config.warmup; r<repetitions; r++) {
			// initialize threads and mutexes
			for(i = 0; i<num_threads; i++) {
				#ifdef COMPILE_WITH_MPI
					int threadid = processid * num_threads + i;
					args[i].iteration_start = threadid*(thread_iterations);
					args[i].iteration_end = (threadid+1)*(thread_iterations);
					if(i == num_threads-1 && processid == num_processes-1) {
						args[i].iteration_end = iterations;
					}
				#else
					args[i].iteration_start = i*(thread_iterations);
					args[i].iteration_end = (i+1)*(thread_iterations);
					if(i == num_threads-1) {
						args[i].iteration_end = iterations;
					}
				#endif

				thread_init_wait(&args[i]);

				if(num_threads > 1) {
					pthread_mutex_lock(&(args[i].start_cond.mutex));
					pthread_mutex_lock(&(args[i].end_cond.mutex));
					pthread_cond_signal(&args[i].start_cond.condition);
				}
			}
			double thread_time_total;
			if(num_threads == 1) {
#ifdef COMPILE_WITH_MPI
				tick_mpi(MODE_START, comm);
#else
				tick(MODE_START);
#endif
				loop_function_ptr(&args[0]);
#ifdef COMPILE_WITH_MPI
				thread_time_total = tick_mpi(MODE_END, comm);
#else
				thread_time_total = tick(MODE_END);
#endif
			}
			else {
#ifdef COMPILE_WITH_MPI
				if(num_processes == 1)
					tick(MODE_START);
				else {
					tick_mpi(MODE_START, comm);
				}
#else
				tick(MODE_START);
#endif

				for(i=0; i<num_threads; i++) {
					pthread_mutex_unlock(&(args[i].start_cond.mutex));
				}

				for(i = 0; i<num_threads; i++) {
					pthread_cond_wait(&(args[i].end_cond.condition), &(args[i].end_cond.mutex));
					pthread_mutex_unlock(&(args[i].end_cond.mutex));
				}
#ifdef COMPILE_WITH_MPI
				if(num_processes == 1) {
					thread_time_total = tick(MODE_END);
				}
				else {
					thread_time_total = tick_mpi(MODE_END, comm);
				}
#else
				thread_time_total = tick(MODE_END);
#endif
			}

			if(r>=0) {
				double speedup = serial_time[r] / thread_time_total;
				calculate_statistics_iterative(&thread_time_total_stat, thread_time_total);
				calculate_statistics_iterative(&speedup_stat, speedup);
			}
			if(r>=config.repetitions.min) {
				double time = thread_time_total_stat.mean * (r + 1);
				if(config.repetitions.time_guide_value > 0 &&
						time>=config.repetitions.time_guide_value) {
					repetitions = r+1;
					break;
				}
			}
		}
		#ifdef COMPILE_WITH_MPI
		MPI_Comm_free(&comm);
		}
		soft_barrier(MPI_COMM_WORLD, 1000);
		#endif
	}

	if(intern_speedup_benchmark) {
		intern_speedup_stat = speedup_stat;
		intern_total_stat = thread_time_total_stat;
		intern_serial_stat = serial_time_stat;
	}
	else {
		print_table_cell("%{processes}2d, ", num_processes);
		print_table_cell("%{threads}2d, ", num_threads);
		print_table_cell("%{iterations}12Lu, ", iterations);
		print_table_cell("%{repetitions}6Lu, ", repetitions);

		print_table_cell("%{serial}" PRECISSION "f,", serial_time_stat.mean);
		print_table_cell("%{serial deviation}" PRECISSION "f, ", serial_time_stat.deviation);

		print_table_cell("%{total}" PRECISSION "f,", thread_time_total_stat.mean);
		print_table_cell("%{total deviation}" PRECISSION "f,", thread_time_total_stat.deviation);
		print_table_cell("%{single}" PRECISSION "f, ", thread_time_total_stat.mean/iterations);

		print_table_cell("%{speedup}" PRECISSION "f,", speedup_stat.mean);
		print_table_cell("%{speedup deviation}" PRECISSION "f, ", speedup_stat.deviation);
		print_table_line();
	}
}

/**
 * Calculate number of repetitions
 */
unsigned speedup_calculate_repetitions(
		unsigned_huge iterations,
		void *(*loop_function_ptr)(void *)) {

	if(config.repetitions.time_guide_value == 0) {
		return config.repetitions.number;
	}
	int repetitions = 1;

	thread_arg_t arg = THREAD_ARG_T_INIT;
	arg.iteration_start = 0;
	arg.iteration_end = iterations;

	tick(MODE_START);
	loop_function_ptr(&arg);
	double time = tick(MODE_END);
	repetitions = config.repetitions.time_guide_value / time;

	if(repetitions < 4) {
		repetitions = 4;
	}


	return repetitions;
}


void compensation_point_benchmark(
		unsigned_huge num_processes,
		unsigned_huge num_threads,
		bool reduce,
		void *(*loop_function_ptr)(void *));

config_t comp_point_config;

unsigned comp_cache_size = 0;
serial_time_cache_t *comp_cache = NULL;

/**
 * parse options for compensation point benchmark and start it
 */
void start_compensation_point_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("compensation point benchmark\n");
	_printf("###############\n");
	comp_point_config = config;
	void *(*loop_function_ptr)(void *) = &simple_integer_arithmetic_loop;
	bool reduce = false;
	print_table_set_additional_info("reduce option", "noreduce");

	char *use_reduction = "reduce", *no_reduction = "noreduce";
	char *loop_fn_int = "int", *loop_fn_float = "float";

	// generate additional table columns
	char *additional_info_header = "reduce option, loop function";
	char *additional_info_reduce = "noreduce, ", *additional_info = NULL;

	get_thread_array(config.threads->end);

	if(option == NULL) option = "int";
	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
	char *token;
	while((token = get_token(&get_token_pointers, option, NULL)) != NULL) {
		// process pthread / loop option
		if(strcmp(token, use_reduction) == 0) {
			reduce = true;
			additional_info_reduce = "reduce, ";
			continue;
		}
		if(strcmp(token, no_reduction) == 0) {
			reduce = false;
			additional_info_reduce = "noreduce, ";
			continue;
		}

		free(additional_info);
		additional_info = NULL;
		strappend(&additional_info, additional_info_reduce);

		if(strcmp(token, loop_fn_int) == 0) {
			loop_function_ptr = &simple_integer_arithmetic_loop;
			strappend(&additional_info, "int, ");
		}
		else if(strcmp(token, loop_fn_float) == 0) {
			loop_function_ptr = &simple_float_arithmetic_loop;
			strappend(&additional_info, "float, ");

		}
		else {
			_printf("WARNING: unknown option for loop benchmark: %s\n", token);
			continue;
		}

		print_table_set_additional_info(additional_info_header, additional_info);


		// start benchmark loop
		for_loop_t process_loop = FOR_LOOP_T_INIT;
		process_loop.var.range = config.processes;
#ifdef COMPILE_WITH_MPI
		process_loop.var.start = config.processes->start;
		if(config.processes->end == 0 || config.processes->end > world_size) {
			process_loop.var.range->end = world_size;
		}
		else {
			process_loop.var.range->end = config.processes->end;
		}
#else
		process_loop.var.range->start = 1;
		process_loop.var.range->end = 1;
#endif
		process_loop.var.name = "process";
		process_loop.step_fn = &step_range;

		for_loop_t thread_loop = FOR_LOOP_T_INIT;
		thread_loop.var.name = "thread";
		thread_loop.var.range = config.threads;
		thread_loop.step_fn = &step_range;
		process_loop.next = &thread_loop;

		int fn(unsigned level, iteration_var_t *vec) {
			unsigned_huge num_processes; get_iteration_value("process", level, vec, &num_processes);
			unsigned_huge num_threads; get_iteration_value("thread", level, vec, &num_threads);
			unsigned_huge iterations; get_iteration_value("iteration", level, vec, &iterations);

			compensation_point_benchmark(num_processes, num_threads,
					reduce, loop_function_ptr);
			return 0;
		}

		int print_header_fn(unsigned level, iteration_var_t *vec) {
			print_header();
			return 0;
		}
		if(process_loop.var.range->end - process_loop.var.range->start <= 1) {
			if(thread_loop.var.range->end - thread_loop.var.range->start <= 1) {
				print_header();
			}
			else {
				thread_loop.outer_start_fn = &print_header_fn;
			}
		}
		else {
			process_loop.outer_start_fn = &print_header_fn;
		}
		nested_for_loop(&process_loop, fn);
	}
	get_token(&get_token_pointers, NULL, NULL);

	free(additional_info);
	config = comp_point_config;
}

/**
 * calculate compensation point, the number of iterations where the speedup is 1
 */
void compensation_point_benchmark(
		unsigned_huge num_processes,
		unsigned_huge num_threads,
		bool reduce,
		void *(*loop_function_ptr)(void *)) {

	unsigned_huge i;
	statistic_t serial_time_mean_stat = STATISTIC_T_INIT;
	statistic_t serial_time_deviation_stat = STATISTIC_T_INIT;

	statistic_t thread_time_total_mean_stat = STATISTIC_T_INIT;
	statistic_t thread_time_total_deviation_stat = STATISTIC_T_INIT;

	statistic_t speedup_mean_stat = STATISTIC_T_INIT;
	statistic_t speedup_deviation_stat = STATISTIC_T_INIT;

	statistic_t iteration_stat = STATISTIC_T_INIT;
	int repetitions = 0;

	unsigned_huge iterations = num_processes * num_threads;
	if(num_threads == 0 ||
		num_processes == 0 ||
		(num_threads == 1 && num_processes == 1)) {
	}
	else {
		repetitions = - ((int)comp_point_config.warmup);
		double start_time;
		while(true) {
			if(repetitions == 0) {
				tick2(MODE_START, &start_time);
			}
			if(repetitions >= (int)comp_point_config.repetitions.min) {
				double tick_end = tick2(MODE_END, &start_time);
#ifdef COMPILE_WITH_MPI
				MPI_Bcast(&tick_end, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
				if(comp_point_config.repetitions.time_guide_value > 0.0 &&
					tick_end > comp_point_config.repetitions.time_guide_value) {
					break;
				}
				if(repetitions >= comp_point_config.repetitions.number) {
					break;
				}
			}
			// calculate number of iterations
			config.repetitions.number = comp_point_config.steps.number;
			config.repetitions.min = comp_point_config.steps.number;
			config.repetitions.time_guide_value = 0;
			/*config.steps.time_guide_value = 0.1;
			config.steps.min = 10;
			config.steps.number = 10;*/
			intern_speedup_benchmark = true;
			iterations = 1000 * num_processes * num_threads;

			unsigned_huge r = 0;
			while(true) {
				if(r>100) break;
				r++;
				serial_time_cache_t *cacheline = get_cache_line(&comp_cache_size, &comp_cache, iterations);

				speedup_benchmark(num_processes, num_threads,
						iterations, reduce, loop_function_ptr, cacheline);

				double ratio = 1 / intern_speedup_stat.mean;
#ifdef COMPILE_WITH_MPI
				MPI_Bcast(&ratio, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
				double diff = 1.0-intern_speedup_stat.mean; if(diff < 0.0) diff = -diff;
#ifdef COMPILE_WITH_MPI
				MPI_Bcast(&diff, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif

				/*_printf("speedup mean %10.5f, dev %10.5f, ", intern_speedup_stat.mean, intern_speedup_stat.deviation);
				_printf("ratio %10.5f, iterations %7d, ", ratio, iterations);
				_printf("p %d, t %d, crn %d, abs %10.5f\n", num_processes, num_threads, config.repetitions.number, diff);
*/
				if(diff < 0.01) {
					//_printf("x");
					config.repetitions.number = comp_point_config.steps.number;
					break;
				}
				//_printf(".");

				if( comp_point_config.steps.time_guide_value > 0 &&
						intern_total_stat.mean > comp_point_config.steps.time_guide_value*1.5) {
					break;
				}
				ratio = ratio < 0 ? 2 : ratio;
				if(ratio < 0.9) ratio = pow(ratio,0.75);
				else if(ratio > 1.1) ratio = pow(ratio,1.25);
				ratio = ratio < 0.5 ? 0.5 : ratio;
				ratio = ratio > 1000 ? 1000 : ratio;

#ifdef COMPILE_WITH_MPI
				MPI_Bcast(&ratio, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
#endif
				iterations *= ratio;
				if(iterations < num_processes * num_threads) iterations = num_processes * num_threads;

			}
			//_printf("\n");

			if(repetitions >= 0) {
				calculate_statistics_iterative(&iteration_stat, (double)iterations);
				calculate_statistics_iterative(&serial_time_mean_stat, intern_serial_stat.mean);
				calculate_statistics_iterative(&serial_time_deviation_stat, intern_serial_stat.deviation);
				calculate_statistics_iterative(&thread_time_total_mean_stat, intern_total_stat.mean);
				calculate_statistics_iterative(&thread_time_total_deviation_stat, intern_total_stat.deviation);
				calculate_statistics_iterative(&speedup_mean_stat, intern_speedup_stat.mean);
				calculate_statistics_iterative(&speedup_deviation_stat, intern_speedup_stat.deviation);
			}

			repetitions++;
		}
	}

	print_table_cell("%{processes}2d, ", num_processes);
	print_table_cell("%{threads}2d, ", num_threads);
	print_table_cell("%{iterations}9.2f, ", iteration_stat.mean);
	print_table_cell("%{iterations deviation}9.2f, ", iteration_stat.deviation);

	print_table_cell("%{repetitions}6d, ", repetitions);

	print_table_cell("%{serial}" PRECISSION "f,", serial_time_mean_stat.mean);
	print_table_cell("%{serial deviation}" PRECISSION "f, ", serial_time_deviation_stat.mean);

	print_table_cell("%{total}" PRECISSION "f,", thread_time_total_mean_stat.mean);
	print_table_cell("%{total deviation}" PRECISSION "f,", thread_time_total_deviation_stat.mean);
	print_table_cell("%{single}" PRECISSION "f, ", thread_time_total_mean_stat.mean/iterations);

	print_table_cell("%{speedup}" PRECISSION "f,", speedup_mean_stat.mean);
	print_table_cell("%{speedup mean dev}" PRECISSION "f, ", speedup_mean_stat.deviation);
	print_table_cell("%{speedup dev mean}" PRECISSION "f, ", speedup_deviation_stat.mean);
	print_table_line();
}
