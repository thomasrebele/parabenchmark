/*
 * memory_benchmark.c
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

#include "definitions.h"
#include "config.h"
#include "timer.h"
#include "statistics.h"
#include "print_functions.h"
#include "memory_benchmark.h"
#include "system_info.h"
#include "getopt.h"
#include "parse.h"
#include "nested_for.h"
#include <sys/types.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdint.h>

#define __USE_GNU
#include <sched.h>
#include <sys/sysinfo.h>


unsigned long long max_malloc_arg();
void mem_clear_steps_cache();
unsigned mem_calculate_repetitions(memory_function_arg_t arg, access_fn_t access_fn);
unsigned mem_calculate_steps(memory_function_arg_t arg, access_fn_t access_fn);
int memory_bandwidth_test(memory_function_arg_t arg, access_fn_t access_fn);

#define MEMORY_MIN_BUFFER_SIZE 128
/**
 * Variables for random number generator.
 * See http://www.metachaos.net/resume/SampleCode3.html
 */
#define RANDOM_A 6364136223846793005
#define RANDOM_C 1442695040888963407

/**
 * iterate over steps, strides and bytes of a blocks, while measuring the time
 */
#define LOOP_SKELETON(accesstype, reginit, \
		strideinit, stridecount, strideaccess, blocksize, \
		intstruction, result) \
	{ \
			register volatile accesstype *buffer_ptr; \
			register volatile accesstype *p; \
			reginit; \
			use_pointer((void*)buffer_ptr); \
			register accesstype tmp  = 0; \
			register unsigned int i; \
			register unsigned int j; \
			register unsigned long long step; \
			step = steps; \
			sched_yield(); \
			tick(MODE_START); \
			while(step-->0) \
			{ \
				strideinit; \
				i = stridecount; \
				while(i-->0) {  \
					strideaccess;  \
					j = blocksize; \
					while(j-->0) {  \
						asm volatile (  \
							intstruction  \
							: [p] "+r" (p), [tmp] "+r" (tmp) \
							:  \
							: "memory"  \
						);  \
						p += 1;  \
					} \
				} \
			} \
			result = tick(MODE_END); \
			use(tmp); \
			use_pointer((void *)p); \
		}
/**
 * execute loop with and without overhead
 */
#define BENCHMARK_SKELETON(accesstype, reginit, \
	strideinit, stridecount, strideaccess, blocksize, \
	overheadinstr, accessinstr, result) \
	static unsigned benchmark_skeleton_pass = 0; \
	void inline1() { \
		LOOP_SKELETON(accesstype, reginit, strideinit, stridecount, strideaccess, blocksize, accessinstr, result.time); \
	} \
	void inline2() { \
		LOOP_SKELETON(accesstype, reginit, strideinit, stridecount, strideaccess, blocksize, overheadinstr, result.overhead);\
	} \
	if(benchmark_skeleton_pass++ %2 == 0) {inline1(); inline2();} else {inline2(); inline1();}


#define ACCESS_TYPE uint32_t
#define REGACCESS "k"

#define READ_OVERHEAD \
	"add %" REGACCESS "[p], %" REGACCESS "[tmp];"
#define READ_ACCESS \
	"add (%[p]), %" REGACCESS "[tmp];"

/* not stable */
#define WRITE_OVERHEAD \
	"mov %" REGACCESS "[tmp], %" REGACCESS "[p];"
#define WRITE_ACCESS \
	"mov %" REGACCESS "[tmp], (%[p]);"

#define READWRITE_OVERHEAD \
	"add %" REGACCESS "[tmp], %" REGACCESS "[p];"
#define READWRITE_ACCESS \
	"add %" REGACCESS "[tmp], (%[p]);"

/**
 * dummy functions to disable optimization
 */
void use(register char var) {
	static char intern_use_static_tmp;
	intern_use_static_tmp += var;
}

void use_pointer(register void *ptr) {
	static long intern_use_pointer_static_tmp;
	intern_use_pointer_static_tmp += (long) ptr;
}

/**
 * Set CPU affinity
 */
void memory_affinity(){
	if(config.thread_affinity != AFFINITY_NONE) {
		cpu_set_t mask;
		CPU_ZERO(&mask);

		int processor_id = get_processorid_recommendation(1);
		CPU_SET(processor_id, &mask);

		int result = sched_setaffinity(0, sizeof(mask), &mask);
		if(result != 0){
			_printf("failed setting cpu affinity\n");
		}
		else {
			_printf("cpu affinity set to 2\n");
		}
	}
}

/**
 * Allocate buffer array
 */
int init_array(unsigned_huge length, volatile void **buffer) {
	// initializing
	if(*buffer != NULL) {
		free((void*)*buffer);
		*buffer = NULL;
	}
	*buffer = (void *) malloc(length);
	if(*buffer == NULL){
		_printf("couldn't allocate %ld bytes\n", length);
		return 1;
	}

	return 0;
}

/**
 * Test for continued read/write
 */
memory_result_t test_continued_readwrite(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize/sizeof(ACCESS_TYPE);
	unsigned_huge stride = arg.stride/sizeof(ACCESS_TYPE);
	unsigned_huge last = datasize/(blocksize+stride);
	if(last == 0 || blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;,
		/* stride init */
		p = buffer_ptr;,
		/* stride count */
		last,
		/* stride access */
		p += stride;,	// p is incremented by macro
		/* block size */
		blocksize,
		/* overhead instruction */
		READWRITE_OVERHEAD,
		/* memory access instruction */
		READWRITE_ACCESS,
		result
		)
	result.transmitted = steps * ((double) last) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = stride * sizeof(ACCESS_TYPE);
	return result;
}

/**
 * Test for continued write
 */
memory_result_t test_continued_write(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize/sizeof(ACCESS_TYPE);
	unsigned_huge stride = arg.stride/sizeof(ACCESS_TYPE);
	unsigned_huge last = datasize/(blocksize+stride);
	if(last == 0 || blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;,
		/* stride init */
		p = buffer_ptr;,
		/* stride count */
		last,
		/* stride access */
		p += stride;,	// p is incremented by macro
		/* block size */
		blocksize,
		/* overhead instruction */
		WRITE_OVERHEAD,
		/* memory access instruction */
		WRITE_ACCESS,
		result
		)

	result.transmitted = arg.steps * ((double) last) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = stride * sizeof(ACCESS_TYPE);
	return result;
}

/**
 * Test for continued read
 */
memory_result_t test_continued_read(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize/sizeof(ACCESS_TYPE);
	unsigned_huge stride = arg.stride/sizeof(ACCESS_TYPE);
	unsigned_huge last = datasize/(blocksize+stride);
	if(last == 0 || blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;,
		/* stride init */
		p = buffer_ptr;,
		/* stride count */
		last,
		/* stride access */
		p += stride;, // p is incremented by macro
		/* block size */
		blocksize,
		/* overhead instruction */
		READ_OVERHEAD,
		/* memory access instruction */
		READ_ACCESS,
		result
		)

	result.transmitted = arg.steps * ((double) last) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = stride * sizeof(ACCESS_TYPE);
	return result;
}

/**
 * Test for random read/write
 */
memory_result_t test_random_readwrite(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize / sizeof(ACCESS_TYPE);
	if(blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}
	unsigned_huge last = datasize/blocksize;
	if(last == 0) {
		result.datasize_enough = false;
		return result;
	}
	last = last - 1; /* (blocksize-1);*/

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;
		volatile register long long x = 0;,
		/* stride init */
		,
		/* stride count */
		(last+1),
		/* stride access */
		x = RANDOM_A*x + RANDOM_C;
		p = buffer_ptr + (last == 0 ? 0 : x % last);,
		/* block size */
		blocksize,
		/* overhead instruction */
		READWRITE_OVERHEAD,
		/* memory access instruction */
		READWRITE_ACCESS,
		result
		)

	result.transmitted = arg.steps * ((double) last+1) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = 0;
	//printf("t %20.20f o %20.20f diff %20.20f \n", result.time, result.overhead, result.time - result.overhead);
	return result;
}

/**
 * Test for random write
 */
memory_result_t test_random_write(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize / sizeof(ACCESS_TYPE);
	if(blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}
	unsigned_huge last = datasize/blocksize;
	if(last == 0) {
		result.datasize_enough = false;
		return result;
	}
	last = last - 1; /* (blocksize-1);*/

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;
		volatile register long long x = 0;,
		/* stride init */
		,
		/* stride count */
		(last+1),
		/* stride access */
		x = RANDOM_A*x + RANDOM_C;
		p = buffer_ptr + (last == 0 ? 0 : x % last);,
		/* block size */
		blocksize,
		/* overhead instruction */
		WRITE_OVERHEAD,
		/* memory access instruction */
		WRITE_ACCESS,
		result
		)

	result.transmitted = arg.steps * ((double) last+1) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = 0;
	//printf("t %20.20f o %20.20f diff %20.20f \n", result.time, result.overhead, result.time - result.overhead);
	return result;
}

/**
 * Test for random read
 */
memory_result_t test_random_read(memory_function_arg_t arg) {
	double time = 0;
	memory_result_t result = MEMORY_RESULT_T_INIT;
	unsigned_huge datasize = arg.data_size/sizeof(ACCESS_TYPE);
	unsigned_huge blocksize = arg.blocksize / sizeof(ACCESS_TYPE);
	if(blocksize == 0) {
		result.datasize_enough = false;
		return result;
	}
	unsigned_huge last = datasize/blocksize;
	if(last == 0) {
		result.datasize_enough = false;
		return result;
	}
	last = last - 1;

	unsigned_huge steps = arg.steps;
	BENCHMARK_SKELETON(
		/* access type */
		ACCESS_TYPE,
		/* register init */
		buffer_ptr = arg.buffer;
		volatile register long long x = 0;,
		/* stride init */
		,
		/* stride count */
		(last+1),
		/* stride access */
		x = RANDOM_A*x + RANDOM_C;
		p = buffer_ptr + (last == 0 ? 0 : x % last);,
		/* block size */
		blocksize,
		/* overhead instruction */
		READ_OVERHEAD,
		/* memory access instruction */
		READ_ACCESS,
		result
		)

	result.transmitted = arg.steps * ((double) last+1) * blocksize * sizeof(ACCESS_TYPE);
	result.datasize_enough = true;
	result.blocksize = blocksize * sizeof(ACCESS_TYPE);
	result.stride = 0;
	//printf("t %20.20f o %20.20f diff %20.20f \n", result.time, result.overhead, result.time - result.overhead);
	return result;
}

/**
 * Allocate arrays for flooding CPU caches
 */
volatile void *cache_clear_memcpya = NULL;
volatile void *cache_clear_memcpyb = NULL;
statistic_t cache_clear_stat = STATISTIC_T_INIT;
void cache_clear_init() {
	unsigned_huge size = config.memory.cache_clean_size;
	init_array(size, &cache_clear_memcpya);
	init_array(size, &cache_clear_memcpyb);
	if(cache_clear_memcpya == NULL || cache_clear_memcpyb == NULL) {
		_printf("WARNING: cache clear couldn't allocate buffer\n");
	}
}

/**
 * Access random elements of two buffers to invalidate cache lines
 */
void cache_clear() {
	// for cleaning cache
	unsigned_huge size = config.memory.cache_clean_size;

	if(cache_clear_memcpya != NULL && cache_clear_memcpyb != NULL) {
		double time;
		tick(MODE_START);
		unsigned_huge i, tmp;
		long long x = 0;
		volatile char *cache_a = (volatile char*) cache_clear_memcpya;
		volatile char *cache_b = (volatile char*) cache_clear_memcpyb;
		for(i=0; i<size; i+=128) {
			x = RANDOM_A*x + RANDOM_C;
			tmp = ((unsigned_huge)x)%size;
			cache_a[tmp] = cache_b[tmp];
			x = RANDOM_A*x + RANDOM_C;
			tmp = ((int)x)%size;
			cache_b[tmp] = cache_a[tmp];
		}
		time = tick(MODE_END);
		calculate_statistics_iterative(&cache_clear_stat, time);
	}
}

void cache_clear_finish() {
	free((void*)cache_clear_memcpya);
	free((void*)cache_clear_memcpyb);
}

/**
 * test name - function map
 * first is default
 */
memory_option_info_t memory_option_infos[] = {
		{"contread", &test_continued_read, true},
		{"cr", &test_continued_read, true},
		{"contwrite", &test_continued_write, true},
		{"cw", &test_continued_write, true},
		{"contreadwrite", &test_continued_readwrite, true},
		{"crw", &test_continued_readwrite, true},
		{"randread", &test_random_read, false},
		{"rr", &test_random_read, false},
		{"randwrite", &test_random_write, false},
		{"rw", &test_random_write, false},
		{"randreadwrite", &test_random_readwrite, false},
		{"rrw", &test_random_readwrite, false},
		{NULL, NULL}
};

/**
 * start memory bandwidth benchmark according to command line options
 */
void start_memory_bandwidth_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("memory bandwidth benchmark\n");
	_printf("bandwidth is Mebibyte / second = 1024*1024 byte / second\n");
	_printf("##############\n");
	cache_clear_init();
	memory_affinity();
	//unsigned_huge min_iterations = config.iterations.start;
	//unsigned_huge max_iterations = config.iterations.end;
	char *additional_info_header = "access function";
	char *additional_info = NULL;

	if(option == NULL) option = "randread";
	if(strcmp(option, "all") == 0) option = "contread,contwrite,contreadwrite,"
			"randread,randwrite,randreadwrite";
	if(strcmp(option, "reversed") == 0) option = "randreadwrite,randwrite,randread,"
			"contreadwrite,contwrite,contread";
	if(strcmp(option, "all2") == 0) option="contread,randread,"
			"contwrite,randwrite,contreadwrite,randreadwrite";
	if(strcmp(option, "reversed2") == 0) option = "randreadwrite,contreadwrite,",
			"randwrite,contwrite,randread,contread";


	if(strcmp(option, "cont") == 0) option = "contread,contwrite,contreadwrite";
	if(strcmp(option, "contrev") == 0) option = "contreadwrite,contwrite,contread";
	if(strcmp(option, "rand") == 0) option = "randread,randwrite,randreadwrite";
	if(strcmp(option, "randrev") == 0) option = "randreadwrite,randwrite,randread";

	// read tests
	unsigned option_count;
	char **options = get_token_array(option, &option_count);

	// set access function using 'nested_for'
	access_fn_t access_fn = &test_random_read;
	bool fn_uses_stride = false;
	int set_access_fn(unsigned level, iteration_var_t *vec) {
		unsigned_huge num_option;
		get_iteration_value("option", level, vec, &num_option);
		if(num_option > option_count) return NESTED_FOR_BREAK;

		mem_clear_steps_cache();
		free(additional_info);
		additional_info = NULL;

		// search for corresponding test
		bool found = false;
		memory_option_info_t *memory_option_ptr = memory_option_infos;
		while(memory_option_ptr->name != NULL) {
			if(strcmp(options[num_option], memory_option_ptr->name) == 0) {
				access_fn = memory_option_ptr->access_fn;
				fn_uses_stride = memory_option_ptr->uses_stride;
				strappend(&additional_info, memory_option_ptr->name);
				strappend(&additional_info, ", ");
				found = true;
				break;
			}
			memory_option_ptr++;
		}
		if(found == false) {
			_printf("WARNING: unknown option for memory benchmark: %s\n", options[num_option]);
			return NESTED_FOR_CONT;
		}
		print_table_set_additional_info(additional_info_header, additional_info);
		return 0;
	}

	bool printed_something = false;
	int after_option_loop(unsigned level, iteration_var_t *vec) {
		if(option_count > 2 && printed_something) _printf("\n");
		return 0;
	}

	// inline function that starts the right test function
	int fn(unsigned level, iteration_var_t *vec) {
		static access_fn_t last_access_fn = NULL;
		static unsigned_huge last_stride = 0;
		static unsigned_huge last_blocksize = 0;

		unsigned_huge j;
		if(get_iteration_value("range", level, vec, &j)) return NESTED_FOR_BREAK;
		unsigned_huge stride;
		if(get_iteration_value("stride", level, vec, &stride)) stride = 0;
		unsigned_huge blocksize;
		if(get_iteration_value("blocksize", level, vec, &blocksize)) blocksize = 32;
		memory_function_arg_t arg;
		arg.data_size = j;
		arg.stride = stride;
		arg.uses_stride = fn_uses_stride;
		arg.blocksize = blocksize;

		if(access_fn != last_access_fn || last_stride != stride || last_blocksize != blocksize) {
			last_access_fn = access_fn; last_stride = stride; last_blocksize = blocksize;
			mem_clear_steps_cache();
		}

		int err;
		if((err = memory_bandwidth_test(arg, access_fn)) < 0) {
			_printf("WARNING: memory_bandwidth_test returned %d with datasize %d\n", j, err);
			return NESTED_FOR_BREAK;
		}
		else if(err > 0) {
			printed_something = true;
		}
		return 0;
	}

	int print_header_fn(unsigned level, iteration_var_t *vec) {
		print_header();
		return 0;
	}

	// exit stride loop, if strides sizes are not supported (e.g. random)
	int check_for_stride_fn(unsigned level, iteration_var_t *vec) {
		if(!fn_uses_stride) return NESTED_FOR_BREAK;
		return 0;
	}

	// loop configuration
	for_loop_t option_loop = FOR_LOOP_T_INIT;
	option_loop.var.name = "option";
	option_loop.var.start = 0;
	option_loop.var.end = option_count;
	option_loop.step_fn = &step_increment;
	option_loop.inner_start_fn = &set_access_fn;
	option_loop.outer_end_fn = &after_option_loop;

	for_loop_t range_loop = FOR_LOOP_T_INIT;
	range_loop.var.name = "range";
	range_loop.var.range = config.range;
	range_loop.step_fn = &step_range;
	range_loop.outer_start_fn = &print_header_fn;

	for_loop_t stride_loop = FOR_LOOP_T_INIT;
	stride_loop.var.name = "stride";
	stride_loop.var.range = config.memory.stride;
	stride_loop.step_fn = &step_range;
	stride_loop.inner_end_fn = &check_for_stride_fn;

	for_loop_t blocksize_loop = FOR_LOOP_T_INIT;
	blocksize_loop.var.name = "blocksize";
	blocksize_loop.var.range = config.memory.blocksize;
	blocksize_loop.step_fn = &step_range;

	option_loop.next = &range_loop;
	range_loop.next = &blocksize_loop;
	blocksize_loop.next = &stride_loop;

	nested_for_loop(&option_loop, fn);

	cache_clear_finish();
	free(additional_info);
}

/**
 * only sqrt(n) results are evaluated
 */
int compare(const void *a, const void *b) {
	double diff = (*(double*)a - *(double*)b);
	return diff > 0 ? 1 : diff < 0 ? -1 : 0;
}

double median(double *array, int size) {
	if(size == 0) return 0;
	qsort(array, size, sizeof(double), compare);

	int middle = size / 2;
	return array[middle];
}

statistic_t middle_stat(double *array, int size) {
	statistic_t result = STATISTIC_T_INIT;
	if(size == 0) return result;
	qsort(array, size, sizeof(double), compare);
	int middle_size = ceil(sqrt(size)/2);
	int middle = size/2;
	int lower = middle - middle_size; lower = lower < 0 ? 0 : lower;
	int upper = middle + middle_size; upper = upper > size ? size : upper;
	int i;
	for(i=lower; i<upper; i++) {
		calculate_statistics_iterative(&result, array[i]);
	}
	return result;
}

/**
 * initialize buffer and repeat test function
 */
int memory_bandwidth_test(memory_function_arg_t arg, access_fn_t access_fn)
{
	unsigned_huge data_size = arg.data_size;
	if(data_size == 0) return 1;
	// initializing
	if(data_size > max_malloc_arg()){
		_printf("cannot test memory bandwidth for %ld bytes, ", data_size);
		_printf("as there are only %ld available\n", max_malloc_arg());
		return -1;
	}

	volatile void *buffer = NULL;
	if(init_array(data_size, &buffer) != 0) {
		_printf("WARNING: couldn't init %d bytes for memory benchmark\n", data_size);
		return -1;
	}
	arg.buffer = buffer;
	char *data_size_str = sprint_num_bytes(data_size);

	unsigned_huge repetitions = mem_calculate_repetitions(arg, access_fn);
	unsigned_huge steps =  mem_calculate_steps(arg, access_fn);

	statistic_t stat_access_time = STATISTIC_T_INIT;
	statistic_t stat_double = STATISTIC_T_INIT;
	statistic_t stat_overhead = STATISTIC_T_INIT;

	statistic_t stat_bandwidth1 = STATISTIC_T_INIT;
	statistic_t stat_bandwidth2 = STATISTIC_T_INIT;

	// do warmup
	double bandwidth;
	double *time_single;
	double *time_overhead;
	int repetitions_int;
	memory_result_t tmp_result = MEMORY_RESULT_T_INIT;
	memory_result_t *result;
	unsigned_huge blocksize = 0, stride = 0;
	tmp_result.datasize_enough = true;
	int r;
	for(r=0; r<config.warmup; r++) {
		memory_result_t tmp_result = MEMORY_RESULT_T_INIT;
		cache_clear();
		arg.steps = steps;
		arg.buffer = buffer;
		tmp_result = access_fn(arg);
		if(!tmp_result.datasize_enough) {
			memory_result_t clean_result = MEMORY_RESULT_T_INIT;
			tmp_result = clean_result;
			tmp_result.datasize_enough = false;
			goto mem_bw_test_finish;
		}
	}

	// do benchmark
	time_single = (double*)malloc((repetitions+1)*sizeof(double));
	time_overhead = (double*)malloc((repetitions+1)*sizeof(double));
	result = (memory_result_t*)malloc((repetitions+1)*sizeof(memory_result_t));
	for(r=0; r<repetitions; r++) {
		memory_result_t empty_result = MEMORY_RESULT_T_INIT;
		result[r] = empty_result;
		cache_clear();
		arg.steps = steps;
		arg.buffer = buffer;
		result[r] = access_fn(arg);
		blocksize = result[r].blocksize;
		stride = result[r].stride;

		calculate_statistics_iterative(&stat_access_time, result[r].time);
		calculate_statistics_iterative(&stat_overhead, result[r].overhead);
		time_single[r] = result[r].time;
		time_overhead[r] = result[r].overhead;

		if(config.repetitions.time_guide_value != 0 && r > config.repetitions.min) {
			double time = (stat_access_time.mean + stat_overhead.mean + cache_clear_stat.mean) * (r + 1);
			if(time > config.repetitions.time_guide_value) {
				repetitions = (r+1);
				break;
			}
		}
	}
	repetitions_int = repetitions;
	stat_access_time = middle_stat(time_single, repetitions_int);
	stat_overhead = middle_stat(time_overhead, repetitions_int);

	free(time_single);
	free(time_overhead);

	// print results
	print_table_cell("%{repetitions}5d, ", repetitions);
	print_table_cell("%{steps}7Lu, ", steps);
	print_table_cell("%{datasize}10Lu, ", data_size);
	print_table_cell("%{bytes}6s, ", data_size_str);

	print_table_cell("%{blocksize}6Lu, ", blocksize);
	print_table_cell("%{stride}6Lu, ", stride);

	print_table_cell("%{step time}" PRECISSION "f, ", stat_access_time.mean/steps);
	print_table_cell("%{step time deviation}" PRECISSION "f, ", stat_access_time.deviation/steps);

	print_table_cell("%{overhead time}" PRECISSION "f, ", stat_overhead.mean/steps);
	print_table_cell("%{overhead time deviation}" PRECISSION "f, ", stat_overhead.deviation/steps);

	bandwidth = result[0].transmitted / (MB * (stat_access_time.mean - stat_overhead.mean));
	if(bandwidth < 0) {
		bandwidth = NAN;
	}
	{
	print_table_cell("%{bandwidth}" BIG_PRECISSION "f, ", bandwidth);
	// bandwidth deviation with gaussian error propagation
	double diff = (stat_access_time.mean - stat_overhead.mean);
	double factor = result[0].transmitted / MB / (diff * diff);
	double sq_access_time_dev_sq = stat_access_time.deviation * stat_access_time.deviation;
	double sq_overhead_dev_sq = stat_overhead.deviation * stat_overhead.deviation;
	double bandwidth_deviation_gauss = factor * sqrt(sq_access_time_dev_sq + sq_overhead_dev_sq);
	print_table_cell("%{bandwidth deviation}" BIG_PRECISSION "f, ", bandwidth_deviation_gauss);
	print_table_cell("%{sample size}4Lu, ", stat_access_time.sample_size);
	}
	print_table_line();

mem_bw_test_finish:
	// free memory
	free((void *)buffer);
	free(data_size_str);
	buffer = NULL;
	if(!tmp_result.datasize_enough) return 0;
	return 1;
}

/**
 * maximal buffer size
 */
unsigned_huge max_malloc_arg(){
	static unsigned_huge size = 0;
	if(size != 0)
		return size;

	struct sysinfo info;
	sysinfo(&info);
	size = info.totalram;
	return size;
}

/**
 * calculate number of steps/repetitions
 */
int mem_index_steps_data = -1;
unsigned_huge mem_index_steps_data_min = 0;
double mem_last_evaluation_time = 0.0f;
unsigned mem_step_data[512];
unsigned mem_repetition_data[512];

void mem_clear_steps_cache() {
	mem_index_steps_data = -1;
}

unsigned mem_calculate_repetitions(
		memory_function_arg_t arg,
		access_fn_t access_fn)
{
	int index = arg.data_size==0 ? 0 : log(arg.data_size) / log(2) + 1;
	if(config.repetitions.time_guide_value == 0) {
		return config.repetitions.number;
	}
	else {
		return 10000;
	}
	if(index < mem_index_steps_data_min) return 0;

	mem_calculate_steps(arg, access_fn);
	if(index < mem_index_steps_data_min) return 0;

	if(index <= mem_index_steps_data) {
		return mem_repetition_data[index];
	}

	unsigned adjustment = pow(2,index-mem_index_steps_data);
	unsigned value = mem_repetition_data[mem_index_steps_data] / adjustment;
	value = value < config.repetitions.min ? config.repetitions.min : value;
	return value;
}

unsigned mem_calculate_steps(
		memory_function_arg_t arg,
		access_fn_t access_fn)
{
	int index = arg.data_size ==0 ? 0 : log(arg.data_size) / log(2) + 1;
	if(index < mem_index_steps_data_min) return 0;
	if(config.steps.time_guide_value == 0) return config.steps.number;
	if(index <= mem_index_steps_data) return mem_step_data[index];
	if(index > mem_index_steps_data && mem_index_steps_data > mem_index_steps_data_min
			&& mem_step_data[mem_index_steps_data] == config.steps.min) return config.steps.min;

	if(index > mem_index_steps_data + 1) {
		unsigned datasize = arg.data_size; arg.data_size /= 2;
		mem_calculate_steps(arg, access_fn);
		arg.data_size = datasize;
	}
	if(cache_clear_stat.sample_size < 1) cache_clear();

	mem_index_steps_data = index;
	int j;
	int steps = 1;
	memory_result_t result;
	int i, max_exp = 32;
	double time;
	for(i=0; i<max_exp; i++) {
		cache_clear();
		arg.steps = steps;
		result = access_fn(arg);
		time = result.time + result.overhead + cache_clear_stat.mean;
		double ratio = (config.steps.time_guide_value - cache_clear_stat.mean) / (result.time + result.overhead);

		if(ratio < 0) {
			steps = 1;
			break;
		}

		if(!result.datasize_enough) {
			mem_index_steps_data_min = index + 1;
			mem_step_data[index] = 0;
			mem_repetition_data[index] = 0;
			return 0;
		}
		if(time > config.steps.time_guide_value*1.5) {
			if(0 < ratio < 1) steps *= ratio;
			break;
		}
		ratio = ratio < 1.2 ? 1.2 : ratio;
		ratio = ratio > 20 ? 20 : ratio;
		if(steps < 10) {
			steps *= 2;
		}
		else {
			steps *= ratio;
		}
	}

	if(steps < config.steps.min) {
		steps = config.steps.min;
	}

	mem_last_evaluation_time = time;

	unsigned repetitions = config.repetitions.time_guide_value / mem_last_evaluation_time;
	repetitions = repetitions <= config.repetitions.min ? config.repetitions.min : repetitions;

	mem_step_data[mem_index_steps_data] = steps;
	mem_repetition_data[mem_index_steps_data] = repetitions;

	return steps;
}
