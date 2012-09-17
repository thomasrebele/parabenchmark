
#include "definitions.h"
#include "config.h"
#include "pthread_benchmark.h"
#include "pthread_functions.h"
#include "timer.h"
#include "statistics.h"
#include "print_functions.h"
#include "getopt.h"
#include "nested_for.h"
#include "system_info.h"
#include "parse.h"

#include <unistd.h>
#include <limits.h>
#include <pthread.h>

extern config_t config;

huge advanced_loop_result(unsigned_huge data_size) {
	int mode = data_size % 4;
	switch(mode) {
	case 0: return 0;
	case 1: return data_size-1;
	case 2: return -1;
	case 3: return -data_size;
	}
	return -1;
}

void* advanced_loop(void *arg) {
	thread_arg_t *args = (thread_arg_t*) arg;
	volatile huge result = 0;
	int factor;
	int i = args->iteration_start;
	for(; i<args->iteration_end; i++) {
		switch((i)%4){
		case 0:
		case 3:
			factor = 1;
			break;
		case 1:
		case 2:
			factor = -1;
			break;
		}
		result += factor * i;
	}
	args->result = result;

	return (void *)NULL;
}

void* simple_integer_arithmetic_loop(void *arg) {
	thread_arg_t *args = (thread_arg_t*) arg;
	unsigned long result = 0;
	int factor;

	unsigned_huge i = args->iteration_start;
	unsigned_huge n = args->iteration_end;

	if(i>=n) return (void *)NULL;
	asm volatile (
		".FORSIAL:"
			"addq %[i], %[r];"
			"incq %[i];"
			"cmpq %[n],%[i];" // GAS syntax
			"jl .FORSIAL;"
			: [r] "+r" (result), [i] "+r" (i)
			: [n] "r" (n)
			);

	args->result = result;
	return (void *)NULL;
}

void* simple_float_arithmetic_loop(void *arg) {
	thread_arg_t *args = (thread_arg_t*) arg;
	volatile float result = 0, add_value = 1.0;
	unsigned_huge i = args->iteration_start;
	unsigned_huge n = args->iteration_end;

	if(i>=n) return (void *)NULL;
	asm volatile(
			"fld %[r];"
		".FORSFAL:"
			"incq %[i];"
			"fld %[a];"
			"faddp;"
			"cmpq %[n],%[i];" // GAS syntax
			"jl .FORSFAL;"

			"fst %[r];"
		: [r] "+m" (result),
		  [i] "+r" (i)

		: [a] "m" (add_value),
		  [n] "r" (n)
		);

	args->result = result;
	return (void *)NULL;
}

void *pthread_exit_wrapper(void *arg) {
	pthread_exit(0);
	return (void*)NULL;
}

void *pthread_stop_time(void *arg) {
	if(arg != NULL) {
		double *time = (double *)arg;
		*time = tick(MODE_END);
	}
	pthread_exit(0);
	return (void*)NULL;
}

void start_pthread_create_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("pthread_create benchmark\n");
	_printf("###############\n");
	_printf("INFO: range option is multiplied minimal possible stack size");

	int states[] = { PTHREAD_CREATE_DETACHED, PTHREAD_CREATE_JOINABLE};

	int state;
	for(state=0; state<2; state++) {
		unsigned_huge i;

		print_header();
		range_reset(config.threads);
		while(range_next(config.threads, &i)) {
			unsigned_huge stacksize_factor;
			range_reset(config.range);
			while(range_next(config.range, &stacksize_factor)) {

				pthread_create_benchmark(i, states[state], stacksize_factor);
			}
		}
	}
}

void pthread_create_benchmark(unsigned num_threads, int detachstate, int stacksize_faktor) {
	pthread_t threads[num_threads];

	int repetitions = config.repetitions.number;

	pthread_attr_t thread_attributes;
	pthread_attr_init(&thread_attributes);
	if(pthread_attr_setstacksize(&thread_attributes, stacksize_faktor * PTHREAD_STACK_MIN)) {
		_printf("WARNING: pthread_attr_setstacksize failed with argument %d\n", stacksize_faktor * PTHREAD_STACK_MIN);
	}
	if(pthread_attr_setdetachstate(&thread_attributes, detachstate)) {
		_printf("WARNING: pthread_attr_setdetachstate failed with argument %d\n", detachstate);
	}

	double timeargs[num_threads];
	statistic_t stat_total = STATISTIC_T_INIT;
	statistic_t stat_single = STATISTIC_T_INIT;
	statistic_t stat_startup = STATISTIC_T_INIT;
	int r = 0;
	while(true) {

		int i;
		double time;

		void *(*pthread_function)(void*);
		pthread_function = &pthread_stop_time;

		for(i=0; i<num_threads; i++) {
			timeargs[i] = 0;
		}

		tick(MODE_START);
		for(i=0; i<num_threads; i++) {
			pthread_create(&threads[i], &thread_attributes, pthread_function, (void *)(&(timeargs[i])));
		}
		time = tick(MODE_END);

		if(detachstate == PTHREAD_CREATE_JOINABLE) {
			for(i=0; i<num_threads; i++) {
				pthread_join(threads[i], NULL);
			}
		}

		statistic_t stat_startup_tmp = STATISTIC_T_INIT;
		for(i=0; i<num_threads; i++) {
			if(detachstate == PTHREAD_CREATE_DETACHED) {
				int max_i = 1000;
				while(timeargs[i] == 0) {
					usleep(100);
					if(i == max_i) break;
				}
			}
			calculate_statistics_iterative(&stat_startup, timeargs[i]);
		}
		calculate_statistics_iterative(&stat_total, time);
		calculate_statistics_iterative(&stat_single, time/num_threads);

		if(r>=config.repetitions.min) {
			double time = stat_total.mean * (r + 1);
			if(config.repetitions.time_guide_value > 0) {
				if(time>=config.repetitions.time_guide_value) {
					repetitions = r+1;
					break;
				}
			}
			if(r>=config.repetitions.number)
				break;
		}
		r++;
	}
	pthread_attr_destroy(&thread_attributes);

	print_table_cell("%{repetitions}6d,", repetitions);
	print_table_cell("%{threads}6d,", num_threads);
	print_table_cell("%{stacksize}10d,", stacksize_faktor * PTHREAD_STACK_MIN);

	print_table_cell("%{total}" PRECISSION "f, ", stat_total.mean);
	print_table_cell("%{total deviation}" PRECISSION "f, ", stat_total.deviation);

	print_table_cell("%{single}" PRECISSION "f, ", stat_single.mean);
	print_table_cell("%{single deviation}" PRECISSION "f, ", stat_single.deviation);

	print_table_cell("%{startup}" PRECISSION "f, ", stat_startup.mean);
	print_table_cell("%{startup deviation}" PRECISSION "f, ", stat_startup.deviation);

	if(detachstate == PTHREAD_CREATE_DETACHED)
		print_table_cell("%{thread detach state}s, ", "detached");
	else
		print_table_cell("%{thread detach state}s, ", "joinable");
	print_table_line();
}



pthread_t *threads ;
void start_pthread_loop_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("pthread loop benchmark\n");
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

		// start benchmark loop
		for_loop_t thread_loop = FOR_LOOP_T_INIT;
		thread_loop.var.name = "thread";
		thread_loop.var.range = config.threads;
		thread_loop.step_fn = &step_range;

		for_loop_t iteration_loop = FOR_LOOP_T_INIT;
		iteration_loop.var.range = config.range;
		iteration_loop.var.name = "iteration";
		iteration_loop.step_fn = &step_range;

		iteration_loop.next = &thread_loop;
		int fn(unsigned level, iteration_var_t *vec) {
			unsigned_huge num_threads; get_iteration_value("thread", level, vec, &num_threads);
			unsigned_huge iterations; get_iteration_value("iteration", level, vec, &iterations);

			pthread_loop_test(num_threads, iterations, reduce, loop_function_ptr);
			return 0;
		}

		int print_header_fn(unsigned level, iteration_var_t *vec) {
			print_header();
			return 0;
		}
		if(thread_loop.var.end - thread_loop.var.start >= 2) {
			thread_loop.outer_start_fn = &print_header_fn;
		}
		else {
			print_header();
		}
		nested_for_loop(&iteration_loop, fn);


	}
	get_token(&get_token_pointers, NULL, NULL);

	free(additional_info);
}

/**
 * threads: number of threads executing test
 * iterations: number of iterations
 * NOTE: threads are initialized with get_thread_array()
 */
void pthread_loop_test(
		unsigned num_threads,
		unsigned_huge iterations,
		bool reduce,
		void *(*loop_function_ptr)(void *)) {

	unsigned_huge i, loop_result;
	statistic_t stat = STATISTIC_T_INIT;
	int repetitions = 0;

	loop_result = 0;
	if(num_threads > iterations || num_threads == 0) {
		stat.mean = NAN;
		stat.deviation = NAN;
	}
	else {
		repetitions = pthread_loop_calculate_repetitions(iterations, loop_function_ptr);

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

		unsigned_huge thread_iterations = iterations / num_threads;
		thread_iterations += iterations % num_threads == 0 ? 0 : 1;

		unsigned cpu_count = get_cpu_count();
		int r=0;
		for(r=-config.warmup; r<repetitions; r++) {
			// initialize threads and mutexes
			for(i = 0; i<num_threads; i++) {
				args[i].iteration_start = i*(thread_iterations);
				args[i].iteration_end = (i+1)*(thread_iterations);
				if(i == num_threads-1) {
					args[i].iteration_end = iterations;
				}
				thread_init_wait(&args[i]);

				pthread_mutex_lock(&(args[i].start_cond.mutex));
				pthread_mutex_lock(&(args[i].end_cond.mutex));
				pthread_cond_signal(&args[i].start_cond.condition);

			}
			double time;

			tick(MODE_START);
			for(i=0; i<num_threads; i++) {
				pthread_mutex_unlock(&(args[i].start_cond.mutex));
			}

			for(i=0; i<num_threads; i++) {
				pthread_cond_wait(&(args[i].end_cond.condition), &(args[i].end_cond.mutex));
				pthread_mutex_unlock(&(args[i].end_cond.mutex));
			}
			time = tick(MODE_END);
			if(r>=0) {
				calculate_statistics_iterative(&stat, time);
			}
			if(r>=config.repetitions.min) {
				if(config.repetitions.time_guide_value > 0) {
					double time = stat.mean * (r + 1);
					if(time > config.repetitions.time_guide_value) {
						repetitions = r+1;
						break;
					}
				}
			}

			loop_result = args[0].result;
		}

	}

	print_table_cell("%{threads}5d, ", num_threads);
	print_table_cell("%{iterations}15Lu, ", iterations);
	print_table_cell("%{repetitions}15Lu, ", repetitions);

	print_table_cell("%{total}" PRECISSION "f, ", stat.mean);
	print_table_cell("%{total deviation}" PRECISSION "f, ", stat.deviation);

	print_table_cell("%{single}" PRECISSION "f, ", stat.mean/iterations);
	print_table_cell("%{single deviation}" PRECISSION "f, ", stat.deviation/iterations);
	print_table_line();
}

unsigned pthread_loop_calculate_repetitions(
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

void comparison(unsigned_huge iterations, void * (*function)(void *)) {
	if(function == NULL)
		function = &thread_function;
	thread_arg_t args;
	args.tid = 1;
	args.iteration_start = 0;
	args.iteration_end = iterations;
	pthread_mutex_init(&(args.start_cond.mutex), NULL);

	int err = pthread_mutex_trylock(&(args.start_cond.mutex));
	if(err != 0) {
		_printf("comparison error trylock %d\n", err);
	}
	else {
		pthread_mutex_unlock(&(args.start_cond.mutex));
	}

	double time;
	tick(MODE_START);
	function(&args);
	time = tick(MODE_END);

	pthread_mutex_destroy(&(args.start_cond.mutex));
	_printf("comparison: i %d, result %d, time %f\n", iterations, args.result, time);
}
