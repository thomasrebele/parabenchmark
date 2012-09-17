/*
 * mpi_benchmark.c
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

#include "definitions.h"
#include "print_functions.h"
#include "mpi_functions.h"
#include "mpi_benchmark.h"
#include "timer.h"
#include "statistics.h"
#include "getopt.h"
#include "parse.h"
#include <mpi.h>

extern int world_rank;
extern int world_size;

STRUCT_ACCESS_ARG(result_network_test_, bandwidth, statistics_arg_network_bandwidth)
STRUCT_ACCESS_ARG(result_network_test_, time, statistics_arg_network_time)

void mpi_reset_rep_and_step_data();
unsigned mpi_calculate_repetitions(mpi_test_t arg);
unsigned mpi_calculate_steps(mpi_test_t arg);

/**
 * Execute MPI_Barrier repeatedly and measure time
 */
double test_barrier(mpi_test_t *arg) {
	unsigned_huge steps = arg->steps;
	int j;
	double time = 0;
	MPI_Status status;

	MPI_Comm comm;
	int rank, size;

	if(create_sub_comm(arg->processes, &comm)) {
		MPI_Comm_size(comm, &size);
		MPI_Barrier(comm);
		tick(MODE_START);
		for(j=0; j<steps; j++) {
			MPI_Barrier(comm);
		}
		time = tick(MODE_END);
		MPI_Barrier(comm);

		double time_result = 0;
		MPI_Reduce(&time, &time_result, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
		time = time_result / size;
		MPI_Comm_free(&comm);
	}
	soft_barrier(MPI_COMM_WORLD, 1000);
	return time;
}

/**
 * send a message with MPI_Send and receive with MPI_Recv,
 * from process 0 to process 1 and back
 * return needed time in seconds
 */
double test_pingpong(mpi_test_t *arg) {
	if(world_size < 2) return NAN;
	int j;
	double time = 0;
	unsigned steps = arg->steps;
	void *buffer = arg->buffer;
	unsigned_huge array_length = arg->array_length;
	MPI_Datatype mpi_type = arg->mpi_type;
	MPI_Status status;
	MPI_Comm comm;

	if(create_sub_comm(arg->processes, &comm)) {
		tick_mpi(MODE_START, comm);
		if(world_rank == 0) {
			for(j = 0; j<steps; j++) {
				MPI_Send(buffer, array_length, mpi_type, 1, TAG_TEST, comm);
				MPI_Recv(buffer, array_length, mpi_type, 1, TAG_TEST, comm, &status);
			}
		}
		else if(world_rank == 1)
		{
			for(j = 0; j<steps; j++) {
				MPI_Recv(buffer, array_length, mpi_type, 0, TAG_TEST, comm, &status);
				MPI_Send(buffer, array_length, mpi_type, 0, TAG_TEST, comm);
			}
		}
		time = tick_mpi(MODE_END, comm);
		time /= 2;
		double time_result = 0;
		MPI_Reduce(&time, &time_result, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
		time = time_result / 2;
		MPI_Comm_free(&comm);
	}
	soft_barrier(MPI_COMM_WORLD, 1000);
	return time;
}

/**
 * Execute MPI_Bcast repeatedly and measure time
 * return needed time in seconds
 */
double test_broadcast(mpi_test_t *arg) {
	unsigned_huge steps = arg->steps;
	void *buffer = arg->buffer;
	unsigned_huge array_length = arg->array_length;
	MPI_Datatype mpi_type = arg->mpi_type;
	int j;
	double time = 0;
	MPI_Status status;

	MPI_Comm comm;
	int rank, size = arg->processes;

	if(create_sub_comm(size, &comm)) {
		tick_mpi(MODE_START, comm);
		for(j=0; j<steps; j++) {
			MPI_Bcast(buffer, array_length, mpi_type, 0, comm);
		}
		time = tick_mpi(MODE_END, comm);
		double time_result = 0;
		MPI_Reduce(&time, &time_result, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
		time = time_result / size;
		MPI_Comm_free(&comm);
	}
	soft_barrier(MPI_COMM_WORLD, 1000);
	return time;
}

/**
 * Execute MPI_Reduce repeatedly and measure time
 * return needed time in seconds
 */
double test_reduce(mpi_test_t *arg) {
	unsigned_huge steps = arg->steps;
	void *buffer = arg->buffer;
	unsigned_huge array_length = arg->array_length;
	void *recvbuffer = (TYPE *) malloc(array_length*sizeof(TYPE));
	MPI_Datatype mpi_type = arg->mpi_type;
	int j;
	double time = 0;
	MPI_Status status;

	MPI_Comm comm;
	int rank, size = arg->processes;

	if(create_sub_comm(size, &comm)) {
		tick_mpi(MODE_START, comm);
		for(j=0; j<steps; j++) {
			MPI_Reduce(buffer, recvbuffer, array_length, mpi_type, MPI_BXOR, 0, comm);
		}
		time = tick_mpi(MODE_END, comm);
		double time_result = 0;
		MPI_Reduce(&time, &time_result, 1, MPI_DOUBLE, MPI_SUM, 0, comm);
		time = time_result / size;
		MPI_Comm_free(&comm);
	}
	soft_barrier(MPI_COMM_WORLD, 1000);
	free(recvbuffer);
	return time;
}

/**
 * test name - function map
 * first is default
 */
mpi_option_info_t mpi_option_infos[] = {
		{"pingpong", true, 2, 2, &test_pingpong},
		{"barrier", false, 1, 0, &test_barrier},
		{"broadcast", true, 2, 0, &test_broadcast},
		{"reduce", true, 2, 0, &test_reduce},
		{NULL, false, 0, 0, NULL}
};

/**
 * process command line options for MPI benchmarks
 */
void start_mpi_bandwidth_benchmark(char *option) {
	_printf("\n### RESULTS ###\n");
	_printf("mpi network_bandwidth benchmark\n");
	_printf("bandwidth is Mebibyte / second = 1024*1024 byte / second\n");
	_printf("##############\n");

	double (*test_function_ptr)(mpi_test_t*) = &test_pingpong;
	print_table_set_additional_info("test function", mpi_option_infos[0].name);

	// generate additional table columns
	char *additional_info_header = "test function";
	char *additional_info = NULL;

	if(option == NULL) option = mpi_option_infos[0].name;
	get_token_t get_token_pointers = GET_TOKEN_T_INIT;
	char *token;
	while((token = get_token(&get_token_pointers, option, NULL)) != NULL) {
		// process pthread / loop option
		free(additional_info);
		additional_info = NULL;

		bool found = false;
		mpi_option_info_t *mpi_option_ptr = mpi_option_infos;
		while(mpi_option_ptr->name != NULL) {
			if(strcmp(token, mpi_option_ptr->name) == 0) {
				test_function_ptr = mpi_option_ptr->test_function_ptr;
				strappend(&additional_info, mpi_option_ptr->name);
				strappend(&additional_info, ", ");
				found = true;
				break;
			}
			mpi_option_ptr++;
		}

		if(found == false) {
			_printf("WARNING: unknown option for loop benchmark: %s\n", token);
			continue;
		}

		print_table_set_additional_info(additional_info_header, additional_info);

		// start benchmark
		mpi_reset_rep_and_step_data();
		_printf("starting mpi network benchmark with %s loop\n", token);
		unsigned_huge i;
		unsigned min_processes = mpi_option_ptr->processes_min;
		unsigned max_processes = mpi_option_ptr->processes_max;
		if(min_processes == 0) {
			min_processes = world_size;
		}
		if(max_processes > world_size) {
			max_processes = world_size;
		}

		print_header();

		mpi_test_t arg = MPI_TEST_T_INIT;
		arg.testfn = test_function_ptr;
		range_reset(config.processes);
		config.processes->start = min_processes > config.processes->start ?
				min_processes : config.processes->start;
		if(max_processes > 0) {
			config.processes->end = max_processes < config.processes->end ?
					max_processes : config.processes->end;
		}
		if(min_processes == max_processes) {
			config.processes->start = min_processes;
			config.processes->end = max_processes;
		}
		while(range_next(config.processes, &i)) {
			if(i<min_processes) i = min_processes;
			if(max_processes>0 && i>max_processes) i = max_processes;
			arg.processes = i;

			if(mpi_option_ptr->uses_data_size == false) {
				arg.data_size = 0;
				mpi_reset_rep_and_step_data();
				network_bandwidth_test(arg);
			}
			else {
				print_header();
				unsigned_huge j;

				range_reset(config.range);
				while(range_next(config.range, &j)) {
					arg.data_size = j;
					int err = network_bandwidth_test(arg);
					if(err < 0) break;
				}

			}
		}
	}
	get_token(&get_token_pointers, NULL, NULL);

	free(additional_info);
}

/**
 * Initialize buffer and start MPI benchmark
 */
int network_bandwidth_test(mpi_test_t arg) {
	unsigned_huge data_size = arg.data_size;

	unsigned_huge array_length = data_size / sizeof(TYPE);
	if((int)array_length < 0) {
		_printf("network_bandwidth_test has got invalid argument %d\n", data_size);
		_printf("(MPI_Send / MPI_Recv convert to int negative)\n");
		return -1;
	}
	arg.array_length = array_length;
	MPI_Status status;
	TYPE *buffer;

	unsigned_huge bytes_sent = array_length * sizeof(TYPE);
	buffer = (TYPE *) malloc(bytes_sent);
	if(buffer == NULL) {
		char *size_str = sprint_num_bytes(bytes_sent);
		_printf("network_bandwidth_test: cannot allocate %Lu bytes (%s) for buffer\n", bytes_sent, size_str);
		free(size_str);
		return -1;
	}
	arg.buffer = buffer;
	soft_barrier(MPI_COMM_WORLD, 1000);

	unsigned repetitions = mpi_calculate_repetitions(arg);
	unsigned long steps = mpi_calculate_steps(arg);
	arg.steps = steps;

	char *data_size_str = sprint_num_bytes(bytes_sent);
	double time;
	unsigned_huge r;
	statistic_t network_bandwidth = STATISTIC_T_INIT;
	statistic_t network_time = STATISTIC_T_INIT;

	if(world_size >= arg.processes) {
		for(r=0; r<config.warmup; r++) {
			arg.testfn(&arg);
		}
		for(r=0; r<repetitions; r++) {
			time = arg.testfn(&arg);
			time /= steps;
			double bandwidth = ((double) (bytes_sent)) / MB / time;
			calculate_statistics_iterative(&network_bandwidth, bandwidth);
			calculate_statistics_iterative(&network_time, time);
		}
	}

	if(world_rank == 0) {
		print_table_cell("%{world size}5d, ", world_size);
		print_table_cell("%{processes}5d, ", arg.processes);
		print_table_cell("%{repetitions}5d, ", repetitions);
		print_table_cell("%{steps}10Lu, ", steps);
		print_table_cell("%{datasize}10Lu, ", data_size);
		print_table_cell("%{bytes}6s, ", data_size_str);

		print_table_cell("%{bandwidth}" PRECISSION "f, ", network_bandwidth.mean);
		print_table_cell("%{bandwidth deviation}" PRECISSION "f, ", network_bandwidth.deviation);

		print_table_cell("%{time}" PRECISSION "f, ", network_time.mean);
		print_table_cell("%{time deviation}" PRECISSION "f, ", network_time.deviation);
		print_table_line();
	}

	free(data_size_str);
	free(buffer);
	//free(results);
	return 0;
}

/**
 * calculate number of steps/repetitions
 */
int mpi_index_steps_data = -1;
double mpi_last_evaluation_time = 0.0f;
unsigned mpi_step_data[512];
unsigned mpi_repetition_data[512];

void mpi_reset_rep_and_step_data() {
	mpi_index_steps_data = -1;
	mpi_last_evaluation_time = 0.0f;
}

unsigned mpi_calculate_repetitions(mpi_test_t arg) {
	if(config.repetitions.time_guide_value == 0) {
		return config.repetitions.number;
	}

	unsigned_huge data_size = arg.data_size;
	mpi_calculate_steps(arg);

	int index = data_size==0 ? 0 : log(data_size) / log(2) + 1;
	if(index <= mpi_index_steps_data) {
		return mpi_repetition_data[index];
	}
	
	unsigned adjustment = pow(2,index-mpi_index_steps_data);
	unsigned value = mpi_repetition_data[mpi_index_steps_data] / adjustment;
	value = value < 4 ? 4 : value;
	return value;
}

unsigned mpi_calculate_steps(mpi_test_t arg) {
	if(config.steps.time_guide_value == 0) {
		return config.steps.number;
	}
	unsigned_huge data_size = arg.data_size;
	void *buffer = arg.buffer;
	int index = data_size==0 ? 0 : log(data_size) / log(2) + 1;
	if(index < 0) {
		return 1;
	}
	if(index <= mpi_index_steps_data) {
		return mpi_step_data[index];
	}

	if(index > mpi_index_steps_data && mpi_step_data[mpi_index_steps_data] == 2) {
		return 2;
	}

	if(index > mpi_index_steps_data + 1) {
		mpi_test_t arg_copy = arg;
		arg_copy.data_size /= 2;
		mpi_calculate_steps(arg_copy);
	}

	mpi_index_steps_data++;
	unsigned_huge array_length = data_size / sizeof(TYPE);
	int j;
	int steps = 1;
	double time;
	while(true) {
		mpi_test_t arg_copy = arg;
		arg_copy.steps = steps;
		time = arg.testfn(&arg_copy);

		MPI_Bcast(&time, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);
		if(time > config.steps.time_guide_value) {
			break;
		}

		steps *= 2;
	}

	if(steps < 4) {
		steps = 4;
	}

	mpi_last_evaluation_time = time;


	unsigned repetitions = config.repetitions.time_guide_value / mpi_last_evaluation_time;
	repetitions = repetitions <= 1 ? 2 : repetitions;

	if(world_rank == 1) {
		steps = 1;
		repetitions = 4;
	}
	// distribute data on all processes
	MPI_Bcast(&steps, 1, MPI_INT, 0, MPI_COMM_WORLD);
	MPI_Bcast(&repetitions, 1, MPI_INT, 0, MPI_COMM_WORLD);

	mpi_step_data[mpi_index_steps_data] = steps;
	mpi_repetition_data[mpi_index_steps_data] = repetitions;

	return steps;
}
