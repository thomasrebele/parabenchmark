/*
 * mpi_functions.h
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

#ifndef __MPI_FUNCTIONS_H
#define __MPI_FUNCTIONS_H

#include <stddef.h>
#include <mpi.h>

enum TAG
{
	TAG_HOSTNAME,
	TAG_CPUMODEL,
	TAG_TEST,
	TAG_SOFT_BARRIER
};

void mpi_functions_init();
void mpi_functions_finalize();
int master_printf(const char* format, ...);
int master_vprintf(const char* format, va_list args);

int create_sub_comm(int size, MPI_Comm *comm);
void soft_barrier(MPI_Comm comm, int useconds);

#endif
