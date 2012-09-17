/*
 * main.c
 *
 * Command line option processing and starting of the benchmarks.
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

#include "print_functions.h"
#include "system_info.h"
#include "definitions.h"
#include "config.h"
#include "getopt.h"
#include "parse.h"
#include "range.h"
#include "nested_for.h"
#include "timer.h"
#include "memory_benchmark.h"
#include "pthread_benchmark.h"
#include "speedup_benchmark.h"
#ifdef COMPILE_WITH_MPI
#include "mpi_functions.h"
#include "mpi_benchmark.h"
extern int world_rank;
#endif

#include <getopt.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
extern int errno;

extern config_t default_config;
extern config_t config;

/**
 * Short option characters and option constants
 */
typedef enum {
	OPT_HELP = 'h',
	OPT_VERBOSE = 'v',

	OPT_OUTPUT_TEE = 'o',
	OPT_OUTPUT_DIR = 'd',
	OPT_OUTPUT_TO_FILES = 'f',
	OPT_SYSTEM_INFO = 'y',
	OPT_NO_STARTUP_SYSTEM_INFO = 'n',

	OPT_REPETITIONS = 'r',
	OPT_STEPS = 's',

	OPT_THREADS = 't',
	OPT_PROCESSES = 'p',
	OPT_RANGE = 'x',
	OPT_BLOCKSIZE = 'b',
	OPT_STRIDE = 'u',

	OPT_WARMUP = 'w',
	OPT_THREAD_AFFINITY = 'a',

	OPT_EXECUTE_TEST = 'e',
	OPT_LIST_TESTS = 'l'
} opt_t;

// option structure for getopt_long()
struct option *long_options;

// option structure for getopt.c
option_info_t option_infos[] = {
	{OPT_EXECUTE_TEST, "comma separated list of tests, which should be executed (default: none)",
			"execute-test", "list_of_tests", required_argument, 0, false},
	{OPT_LIST_TESTS, "print list of tests which are available",
			"list-tests", "text", no_argument, 0, true},

	{OPT_PROCESSES, "set number of processes",
			"processes", "range[,range...]", required_argument, 0, false},
	{OPT_THREADS, "set number of threads",
			"threads", "range[,range...]", required_argument, 0, false},
	{OPT_RANGE, "general range (can be data size/iterations, dependent on selected benchmark)",
			"range", "range[,range...]", required_argument, 0, false},
	{OPT_THREAD_AFFINITY, "thread affinity (default: roundrobin)",
			"thread-affinity", "none|roundrobin", required_argument, 0, true},

	{OPT_REPETITIONS, "the program tries to use only 'arg' seconds for ALL repetitions",
			"repetitions", "min[float],time[float]", required_argument, 0, false},
	{OPT_STEPS, "the program tries to use only 'arg' seconds for ONE repetition",
			"steps", "min[float],time[float]", required_argument, 0, false},
	{OPT_WARMUP, "the number of repetitions which don't affect statistics (default: 1)",
			"warmup", "int", required_argument, 0, true},


	{OPT_BLOCKSIZE, "block size for memory benchmark",
			"blocksize", "range[,range...]", required_argument, 0, false},
	{OPT_STRIDE, "stride for memory benchmark",
			"stride", "range[,range...]", required_argument, 0, false},

	{OPT_OUTPUT_TEE, "benchmark output also on screen when writing to files (default: true)",
				"output-tee", "true|false", optional_argument, 0, false},
	{OPT_OUTPUT_TO_FILES, "output each test to a separate file, else to stdout (default: true)",
			"output-to-files", "true|false", optional_argument, 0, false},
	{OPT_OUTPUT_DIR, "output log files to directory (default: log)",
				"output-dir", "path", required_argument, 0, false},
	{OPT_SYSTEM_INFO, "print system info (default: true)",
				"print-system-info", "true|false", required_argument, 0, false},
	{OPT_NO_STARTUP_SYSTEM_INFO, "omit system info on startup (default: false)",
				"no-startup-system-info", "true|false", required_argument, 0, true},

	{OPT_VERBOSE, "print more informative messages",
			"verbose", "", no_argument, 0, false},
	{OPT_HELP, "print this help message",
			"help", "", no_argument, 0, false},
	{0, NULL, NULL, NULL, 0, false}
};

/**
 * benchmark name - start function map
 */
typedef struct test_t_ {
	char* name;
	void (*start_function)(char *option);
	char* description;
} test_t;

test_t tests[] = {
		{"memory-bandwidth", &start_memory_bandwidth_benchmark, ""}, // TODO: Test description
		{"pthread-create", &start_pthread_create_benchmark, ""}, // TODO: Test description
		{"pthread-loop", &start_pthread_loop_benchmark, "option (list): int, float"},
		{"speedup", &start_speedup_benchmark, "option (list): int, float"},
		{"compensation-point", &start_compensation_point_benchmark, "option (list): int, float"},
#ifdef COMPILE_WITH_MPI
		{"mpi-bandwidth", &start_mpi_bandwidth_benchmark, ""},
#endif
		{NULL, NULL, NULL}
};

/**
 * main function
 */
int main (int argc, char **argv) {
#ifdef COMPILE_WITH_MPI
	mpi_functions_init();
	set_vprintf_function(&master_vprintf);
	_printf("set to master\n");
#endif

	char *short_options;
	long_options = getopt_init(option_infos, &short_options);

	// set defaut values
	default_config.output_tee = true;
	default_config.output_to_files = true;
	default_config.output_dir = "log";
	default_config.output_system_info = true;
	default_config.output_omit_startup_system_info = false;

	default_config.thread_affinity = AFFINITY_ROUND_ROBIN;
	default_config.warmup = 1;
	//default_config.steps.time_guide_value = 0.06;
	//default_config.repetitions.time_guide_value = 1;
	default_config.steps.number = 100;
	default_config.steps.min = 10;
	default_config.repetitions.number = 100;
	default_config.repetitions.min = 10;

	default_config.processes = parse_range_option("1-4[*2]");
	default_config.threads = parse_range_option("1-16[*2]");
	default_config.range = parse_range_option("1-10000000000[*2]");
	default_config.memory.stride = parse_range_option("1-512[*2]");
	default_config.memory.blocksize = parse_range_option("1-512[*2]");

	default_config.memory.cache_clean_size = 8*MB;

	// process command line options
    int c;
    int i, j;
    double double_value;
    int int_value;

    char *run_tests = "";

    int option_index = 0;
    while ((c = getopt_long(argc, argv, short_options,
                 long_options, &option_index)) != -1) {

        int this_option_optind = optind ? optind : 1;
        switch (c) {
        case 0:
            _printf ("LONG option %s", long_options[option_index].name);
            if (optarg)
                _printf (" with arg %s", optarg);
            printf ("\n");
            break;

        case OPT_LIST_TESTS: {
        	test_t *test = tests;
        	_printf("\n\tavailable tests:\n");
        	while(test->name != NULL) {
        		_printf("\t\t%s\t\n", test->name, test->description);

        		test++;
        	}
        	_printf("\n");
			#ifdef COMPILE_WITH_MPI
				mpi_functions_finalize();
			#endif
        	exit(0);
        	break;
        }

        case OPT_EXECUTE_TEST:
        	run_tests = optarg;
        	break;

        case OPT_PROCESSES:
			default_config.processes = parse_range_option(optarg);
			break;

        case OPT_THREADS:
        	default_config.threads = parse_range_option(optarg);
    		break;

        case OPT_RANGE:
        	default_config.range = parse_range_option(optarg);
            break;

        case OPT_BLOCKSIZE:
        	default_config.memory.blocksize = parse_range_option(optarg);
            break;

        case OPT_STRIDE:
        	default_config.memory.stride = parse_range_option(optarg);
        	break;

        case OPT_REPETITIONS: {
        	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
			char *option, *token;
			while((token = get_token(&get_token_pointers, optarg, &option)) != NULL) {
				if(strcmp(token, "time") == 0) {
					double_value = atof(option);
					default_config.repetitions.time_guide_value = double_value;
				}
				else if(strcmp(token, "number") == 0) {
					int_value = atoi(option);
					default_config.repetitions.number = int_value;
					default_config.repetitions.time_guide_value = 0;
				}
				else if(strcmp(token, "min") == 0) {
					int_value = atoi(option);
					default_config.repetitions.min = int_value;
				}
				else {
					_printf("WARNING: repetitions option %s not valid\n", token);
				}
			}

        	break;
        }
        case OPT_STEPS: {
        	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
			char *option, *token;
			while((token = get_token(&get_token_pointers, optarg, &option)) != NULL) {
				if(strcmp(token, "time") == 0) {
					double_value = atof(option);
					default_config.steps.time_guide_value = double_value;
				}
				else if(strcmp(token, "number") == 0) {
					int_value = atoi(option);
					default_config.steps.number = int_value;
					default_config.steps.time_guide_value = 0;
				}
				else if(strcmp(token, "min") == 0) {
					int_value = atoi(option);
					default_config.steps.min = int_value;
				}
				else {
					_printf("WARNING: steps option %s not valid\n", token);
				}
			}
			break;
        }

        case OPT_THREAD_AFFINITY: {
        	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
			char *option, *token;
			while((token = get_token(&get_token_pointers, optarg, &option)) != NULL) {
				if(strcmp(token, "none") == 0) {
					default_config.thread_affinity = AFFINITY_NONE;
				}
				else if(strcmp(token, "roundrobin") == 0) {
					default_config.thread_affinity = AFFINITY_ROUND_ROBIN;
				}
				else {
					_printf("WARNING: thread affinity option %s not valid\n", token);
				}
			}
        	break;
        }

        case OPT_WARMUP:
        	default_config.warmup =  atoi(optarg);
        	break;

        case OPT_OUTPUT_TEE:
        	parse_bool_option("output tee", optarg, &default_config.output_tee, true);
        	break;
        case OPT_OUTPUT_TO_FILES:
        	parse_bool_option("output to files", optarg, &default_config.output_to_files, true);
        	break;
        case OPT_SYSTEM_INFO:
        	parse_bool_option("output system info", optarg, &default_config.output_system_info, true);
        	break;
        case OPT_NO_STARTUP_SYSTEM_INFO:
        	parse_bool_option("no startup system info", optarg, &default_config.output_omit_startup_system_info, true);
        	break;

        case OPT_OUTPUT_DIR: {
        	default_config.output_dir = optarg;
        	_printf("setting output dir %s\n", default_config.output_dir);
        	struct stat status;
        	int err = stat(default_config.output_dir, &status);
        	if(err == 0) {
        		if(!(status.st_mode & S_IFDIR)) {
        			_printf("ERROR: %s is not a directory\n");
        		}
        	}
        	else if(errno == ENOENT) {
        		err = mkdir(default_config.output_dir, S_IRWXU);
        		if(err != 0) {
        			_printf("ERROR: directory %s couldn't be created\n");
        			default_config.output_dir = "";
        		}
        	}
        	break;
        }

        case OPT_VERBOSE:
        	default_config.verbose++;
        	break;

        case '?':
        case OPT_HELP:
        	print_usage(argv[0]);
			#ifdef COMPILE_WITH_MPI
				mpi_functions_finalize();
			#endif
        	exit(0);
            break;
        default:
        	_printf("?? getopt returned character code %d ??\n", c);
            break;
        }
    }
    if(default_config.verbose>1) {
		if (optind < argc) {
			printf ("non-option ");
			int tmp_optind = optind;
			while (tmp_optind < argc)
				_printf("%s ", argv[tmp_optind++]);
			_printf("\n");
		}
    }


    if(default_config.verbose>1) {
		if(default_config.output_to_files) {
			_printf("redirect test output to files\n");
		}
		if(default_config.output_to_files) {
			_printf("printing to separate files\n");
		}
		else {
			_printf("printing to stdout\n");
		}
    }

    if(default_config.threads->start == 0) {
    	_printf("WARNING: Tests have to run with at least 1 thread!\n");
    }

    if((default_config.repetitions.time_guide_value == 0 ||
    		default_config.steps.time_guide_value == 0) &&
    		default_config.repetitions.time_guide_value !=
    				default_config.steps.time_guide_value) {
    	_printf("WARNING: mixing number / time arguments for repetitions / steps possibly not implemented\n");
    }

    if(default_config.output_system_info
    		&& !default_config.output_omit_startup_system_info) {
    	config = default_config;
    	print_system_info();
    }
	fetch_cpu_info();
	calibrate_timer();

    _printf("run tests argument: %s\n", run_tests);

    // start tests
    get_token_t token_interna = GET_TOKEN_T_INIT;
    char *option, *token;
	while((token = get_token(&token_interna, run_tests, &option)) != NULL) {
		config = default_config;
		test_t *test = tests;

		// search corresponding test
		bool found_test = false;
		while(test->name != NULL) {
			if(strcmp(test->name, token) == 0) {
				found_test = true;

				if(default_config.output_to_files == true) {
#ifdef COMPILE_WITH_MPI
					get_hostname(); // get hostname uses barrier on MPI_COMM_WORLD
					if(world_rank == 0) {
#endif
					// create filename
					char filename[200];
					time_t t = time(NULL);
					struct tm tm = *localtime(&t);
					char *option_str = option;
					if(option == NULL) option_str = "";
					sprintf(filename, "%s-%s[%s]-%04d-%02d-%02d-%02d.%02d.%02d.log",
							get_hostname(), token, option_str,
							tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
							tm.tm_hour, tm.tm_min, tm.tm_sec);
					set_output_to_file(filename, "w");
					if(default_config.output_system_info) {
						print_system_info();
					}
#ifdef COMPILE_WITH_MPI
					}
#endif
				}

				_printf("got argument for test %s, ", token, option);
				_printf("option %s\n", option == NULL? "(none)" : option);

				test->start_function(option);
			}
			test++;
		}

		if(!found_test) {
			_printf("WARNING: no test \"%s\" specified. It will be ignored\n", token);
		}
	}
	get_token(&token_interna, NULL,NULL);

	#ifdef COMPILE_WITH_MPI
		mpi_functions_finalize();
	#endif

    exit (0);
}
