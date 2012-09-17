/*
 * mpi_functions.c
 *
 * Helper functions for MPI
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
#include "mpi_functions.h"
#include <mpi.h>
#include <unistd.h>

int world_size;
int world_rank;

/**
 * Init
 */
void mpi_functions_init(int argc, char **argv) {
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
}

/**
 * print only on master
 */
int master_printf(const char* format, ...) {
	int result = 0;
	if(world_rank == 0) {
		va_list args;
		va_start(args,format);
		result = vprintf(format,args);
		va_end(args);
	}
	return result;
}

/**
 * vprint only on master
 */
int master_vprintf(const char* format, va_list args) {
	int result = 0;
	if(world_rank == 0) {
		result = vprintf(format,args);
	}
	return result;
}

/**
 * print with prefix "rank " and number
 */
void rank_printf(char* format, ...) {
	printf("rank %d, ", world_rank);
	va_list args;
	va_start(args,format);
	vprintf(format,args);
	va_end(args);
}

void mpi_functions_finalize() {
	MPI_Finalize();
}

/**
 * create communicator with processes 0,...,size-1
 * return 1 if successful
 */
int create_sub_comm(int size, MPI_Comm *comm) {
	if(size > world_size) return 0;

	MPI_Group world_group, sub_group;
	int ranks[size];
	int i;
	for(i=0; i<size; i++) {
		ranks[i] = i;
	}
	MPI_Comm_group(MPI_COMM_WORLD, &world_group);
	MPI_Group_incl(world_group, size, ranks, &sub_group);
	MPI_Comm_create(MPI_COMM_WORLD, sub_group, comm);
	MPI_Group_free(&sub_group);
	if(*comm == MPI_COMM_NULL) {
		return 0;
	}
	return 1;
}

/**
 * MPI barrier with less CPU usage (for non-time-critical usage only!)
 */
void soft_barrier(MPI_Comm comm, int useconds) {
	int rank, size; int dummy = 0; int flag = 0;
	MPI_Comm_rank(comm, &rank);
	MPI_Comm_size(comm, &size);
	if(rank == 0) {
		int i;
		MPI_Request req[size-1];
		for(i=1; i<size; i++) {
			MPI_Issend(&dummy, 1, MPI_INT, i, TAG_SOFT_BARRIER, comm, &req[i-1]);
		}
		MPI_Testall(size-1, req, &flag, MPI_STATUSES_IGNORE);
		while(!flag) {
			usleep(useconds);
			MPI_Testall(size-1, req, &flag, MPI_STATUSES_IGNORE);
		}
	}
	else {
		MPI_Request req;
		MPI_Irecv(&dummy, 1, MPI_INT, 0, TAG_SOFT_BARRIER, comm, &req);
		MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
		while(!flag) {
			usleep(useconds);
			MPI_Test(&req, &flag, MPI_STATUS_IGNORE);
		}
	}
}
