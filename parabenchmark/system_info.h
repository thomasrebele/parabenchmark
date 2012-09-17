/*
 * system_info.h
 *
 * Collect and print information of CPU, memory, running processes and MPI
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

#ifndef __SYSTEM_INFO_H
#define __SYSTEM_INFO_H

typedef struct {
	double cpuinfo_cur_freq;
	double cpuinfo_max_freq;
	double cpuinfo_min_freq;
	double scaling_cur_freq;
	double scaling_max_freq;
	double scaling_min_freq;
} sys_cpu_t;

typedef struct {
	int processor_id;
	char *vendor;
	int cpu_family;
	int model;
	char *model_name;
	char *stepping;
	double cpuinfo_hz;
	sys_cpu_t sys;
	int physical_id;
	int siblings;
	int core_id;
	int cpu_cores;
} cpuinfo_t;

typedef struct {
	cpuinfo_t **cpuinfos;
	int cpu_size;
} core_t;

typedef struct {
	core_t *cores;
	int processor_size;
} processor_t;


typedef struct {
	unsigned mem_total;
	unsigned mem_free;
} meminfo_t;

char* get_hostname();
unsigned get_cpu_count();
unsigned get_processorid_recommendation(unsigned i);
float get_cpu_frequency(int);

void fetch_cpu_info();
void print_system_info();
void print_cpu_info();
void print_mpi_info();
void print_top_info();
void print_config_info();


#endif
