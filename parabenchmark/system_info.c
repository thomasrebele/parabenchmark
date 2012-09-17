/*
 * system_info.c
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

#include "definitions.h"
#include "system_info.h"
#include "print_functions.h"
#include "git_ref.h"
#include "range.h"
#include "parse.h"
#include <stdio.h>
#include <sys/sysinfo.h>
#ifdef COMPILE_WITH_MPI
#include <mpi.h>
#include "mpi_functions.h"
#include "mpi_ref.h"
extern int world_rank;
#endif

#define INTERN
#ifdef EXTERN
#undef INTERN
#endif

#include "config.h"


cpuinfo_t *cpuinfos = NULL;
int cpuinfos_size = 0;

processor_t *processors = NULL;
int processors_size = 0;

char **hostname;
int hostname_size = 0;

char **cpuworld;

/**
 * parse information of CPU frequency
 */
sys_cpu_t fetch_sys_cpu_info(int processorid) {
	sys_cpu_t result;
	typedef struct tmp { char *name; double *var; } tmp_t;
	tmp_t keys[] = {
			{"cpuinfo_cur_freq", &(result.cpuinfo_cur_freq)},
			{"cpuinfo_max_freq", &(result.cpuinfo_max_freq)},
			{"cpuinfo_min_freq", &(result.cpuinfo_min_freq)},
			{"scaling_cur_freq", &(result.scaling_cur_freq)},
			{"scaling_max_freq", &(result.scaling_max_freq)},
			{"scaling_min_freq", &(result.scaling_min_freq)},
			{NULL, NULL}};

	tmp_t *act_key = keys;
	while(act_key->name != NULL) {
		char filename[1024];
		sprintf(filename, "/sys/devices/system/cpu/cpu%d/cpufreq/%s", processorid, act_key->name);
		FILE *fh = fopen(filename, "r");
		if(fh == NULL) {
			*(act_key->var) = NAN;
		}
		else {
			char buffer[1024];
			if(fgets(buffer, sizeof(buffer)-1, fh) != NULL) {
				*(act_key->var) = atof(buffer)*1000;
			}
			else {
				*(act_key->var) = NAN;
			}
		}
		act_key++;
	}
	return result;
}

/**
 * parse information of /proc/cpuinfo and save it to arrays cpuinfos and processors
 */
void fetch_cpu_info() {
	if(cpuinfos_size != 0) return;

	FILE *output = popen("cat /proc/cpuinfo", "r");
	if(!output) return;

	char buffer[1024];
	int j;
	for(j=0; j<1024; j++) {
		buffer[j] = 0;
	}
	int act_processor = -1;

	while(fgets(buffer, sizeof(buffer)-1, output) != NULL) {
		//printf("%s", buffer);
		char *ptr = buffer;
		char *key = NULL;
		char *value = NULL;
		while(*ptr != 0) {
			if(*ptr == ':') {
				*ptr = 0;
				key = buffer;
				right_trim(key);
				value = ptr+2;
			}
			ptr++;
		}
		if(key != NULL) {
			if(strcmp(key, "processor") == 0) {
				act_processor ++;
				if(cpuinfos_size <= act_processor) {
					cpuinfos_size = 2*act_processor + 1;
					cpuinfos = (cpuinfo_t*)realloc(cpuinfos, cpuinfos_size*sizeof(cpuinfo_t));
				}
				int processor_id = atoi(value);
				cpuinfos[act_processor].processor_id = atoi(value);
				cpuinfos[act_processor].sys = fetch_sys_cpu_info(processor_id);
			}
			if(act_processor < 0) continue;
			char **dest = NULL;
			if(strcmp(key, "vendor_id") == 0)
				dest = &(cpuinfos[act_processor].vendor);
			else if(strcmp(key, "model name") == 0)
				dest = &(cpuinfos[act_processor].model_name);
			else if(strcmp(key, "stepping") == 0)
				dest = &(cpuinfos[act_processor].stepping);
			else if(strcmp(key, "cpu family") == 0)
				cpuinfos[act_processor].cpu_family = atoi(value);
			else if(strcmp(key, "model") == 0)
				cpuinfos[act_processor].model = atoi(value);
			else if(strcmp(key, "cpu MHz") == 0)
				cpuinfos[act_processor].cpuinfo_hz = atof(value)*1000000;
			else if(strcmp(key, "physical id") == 0)
				cpuinfos[act_processor].physical_id = atoi(value);
			else if(strcmp(key, "siblings") == 0)
				cpuinfos[act_processor].siblings = atoi(value);
			else if(strcmp(key, "core id") == 0)
				cpuinfos[act_processor].core_id = atoi(value);
			else if(strcmp(key, "cpu cores") == 0)
				cpuinfos[act_processor].cpu_cores = atoi(value);

			if(dest != NULL) {
				right_trim(value);
				char *str = (char*)calloc((strlen(value)+2),sizeof(char));
				strcpy(str, value);
				*dest = str;
			}
		}
	}
	cpuinfos_size = act_processor+1;

	int i;
	for(i=0; i<cpuinfos_size; i++) {
		int pid = cpuinfos[i].physical_id;
		int cid = cpuinfos[i].core_id;
		// create element in 'processors' if missing
		if(pid >= processors_size) {
			processors = (processor_t*) realloc(processors, (pid + 1) * sizeof(processor_t));
			int j;
			for(j=processors_size; j<=pid; j++) {
				processors[j].cores = NULL;
				processors[j].processor_size = 0;
			}
			processors_size = pid + 1;
		}
		// create element in 'processors[pid].cores' if missing
		if(cid >= processors[pid].processor_size) {
			processors[pid].cores = (core_t*) realloc(processors[pid].cores, (cid + 1) * sizeof(core_t));
			int j;
			for(j=processors[pid].processor_size; j<=cid; j++) {
				processors[pid].cores[j].cpuinfos = NULL;
				processors[pid].cores[j].cpu_size = 0;
			}
			processors[pid].processor_size = cid + 1;
		}
		// add item to processors[pid].cores[cid].cpuinfos
		unsigned cpu_size = processors[pid].cores[cid].cpu_size;
		unsigned newsize = cpu_size + 1;
		processors[pid].cores[cid].cpuinfos = (cpuinfo_t**)realloc(
				processors[pid].cores[cid].cpuinfos,
				newsize*sizeof(cpuinfo_t*));
		processors[pid].cores[cid].cpuinfos[cpu_size] = &cpuinfos[i];
		processors[pid].cores[cid].cpu_size++;
	}
}

/**
 * get hostname. If MPI is used, gather all hostnames at process 0
 */
char* get_hostname() {
	if(hostname_size == 0) {
#ifdef COMPILE_WITH_MPI
		fetch_cpu_info();
		int rank;
		MPI_Comm_size(MPI_COMM_WORLD, &hostname_size);
		MPI_Comm_rank(MPI_COMM_WORLD, &rank);
		hostname = (char**)calloc(hostname_size,sizeof(char*));
		cpuworld = (char**)calloc(hostname_size,sizeof(char*));
		cpuworld[0] = (char*)calloc(1024, sizeof(char));
		hostname[0] = (char*)calloc(1024, sizeof(char));
		if(cpuinfos_size > 0) {
			strcpy(cpuworld[0], cpuinfos[0].model_name);
		}
		else {
			strcpy(cpuworld[0], "");
		}

		FILE *output = popen("hostname", "r");
		if(!output) return "";
		if(fgets(hostname[0], 1024-1, output) != NULL) {
			right_trim(hostname[0]);
		}

		if(rank == 0) {
			int i;
			for(i=1; i<hostname_size; i++) {
				hostname[i] = (char*)calloc(1024,sizeof(char));
				MPI_Recv(hostname[i], 1023, MPI_CHAR, i, TAG_HOSTNAME, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
				cpuworld[i] = (char*)calloc(1024,sizeof(char));
				MPI_Recv(cpuworld[i], 1023, MPI_CHAR, i, TAG_CPUMODEL, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
			}
		}
		else {
			MPI_Send(hostname[0], 1023, MPI_CHAR, 0, TAG_HOSTNAME, MPI_COMM_WORLD);
			MPI_Send(cpuworld[0], 1023, MPI_CHAR, 0, TAG_CPUMODEL, MPI_COMM_WORLD);
		}
#else
		hostname_size = 1;
		hostname = (char**)calloc(hostname_size,sizeof(char*));
		hostname[0] = (char*)calloc(1024,sizeof(char));

		FILE *output = popen("hostname", "r");
		if(!output) return "";
		if(fgets(hostname[0], 1024-1, output) != NULL) {
			right_trim(hostname[0]);
		}
#endif
	}

	return hostname[0];
}

/**
 * print content configuration variables
 */
void print_config_info() {
	_printf("all config info (effect depends possibly on selected benchmark):\n");
	_printf("\toutput to files=%s;\n", BOOL_STR(config.output_to_files));
	_printf("\toutput system info=%s;\n", BOOL_STR(config.output_to_files));
	_printf("\tomit startup system info=%s;\n", BOOL_STR(config.output_omit_startup_system_info));
	_printf("\toutput dir=%s;\n", config.output_dir);

	_printf("\tthread affinity=");
	switch (config.thread_affinity) {
	case AFFINITY_NONE: _printf("none"); break;
	case AFFINITY_ROUND_ROBIN: _printf("roundrobin"); break;
	}
	_printf(";\n");

	_printf("\trepetitions time guide value=%15.11f, ", config.repetitions.time_guide_value);
	_printf("number value=%d, ", config.repetitions.number);
	_printf("min=%d;\n", config.repetitions.min);
	_printf("\tsteps time guide value=%15.11f, ", config.steps.time_guide_value);
	_printf("number value=%d, ", config.steps.number);
	_printf("min=%d;\n", config.steps.min);

	_printf("\tprocesses:\n"); range_print("\t\t", config.processes);
	_printf("\tthreads:\n"); range_print("\t\t", config.threads);
	_printf("\titerations:\n"); range_print("\t\t", config.range);

	_printf("\tverbose level=%d;\n", config.verbose);
}

/**
 * print information about CPU. Only first 5 processors are printed
 * (to avoid massive output e.g. on SGI Altix systems
 */
void print_cpu_info() {
	_printf("\tcpuinfo:\n");

	fetch_cpu_info();
	int i;

	int processor_end = processors_size;
	if(processors_size > 5) processor_end = 5;
	for(i=0; i<processor_end; i++) {
		processor_t  processor = processors[i];
		if(processor.processor_size == 0) {
			if(processor_end < processors_size) processor_end++;
			continue;
		}
		cpuinfo_t cpu = *(processor.cores[0].cpuinfos[0]);
		_printf("\t*\tphysical id=%d, vendor=%s;\n", cpu.physical_id, cpu.vendor);
		_printf("\t\tcpu family=%d, model name=%s;\n", cpu.cpu_family, cpu.model_name);
		_printf("\t\tcpu cores=%d, siblings=%d, stepping=%s;\n", cpu.cpu_cores, cpu.siblings, cpu.stepping);
		int j;
		for(j=0; j<processor.processor_size; j++) {
			core_t core = processor.cores[j];
			int k;
			for(k=0; k<core.cpu_size; k++) {
				cpu = *(core.cpuinfos[k]);
				_printf("\t\t*\t");
				_printf("processor id=%d, ", cpu.processor_id);
				_printf("core id=%d, ", cpu.core_id);
				_printf("cpuinfo Hz=%.0f", cpu.cpuinfo_hz);
				_printf("\n");

				if(!(isnan(cpu.sys.cpuinfo_cur_freq) &&
						isnan(cpu.sys.cpuinfo_min_freq) &&
						isnan(cpu.sys.cpuinfo_max_freq))) {
					_printf("\t\t\t");
					_printf("cpuinfo cur=%.0f, ", cpu.sys.cpuinfo_cur_freq);
					_printf("cpuinfo min=%.0f, ", cpu.sys.cpuinfo_min_freq);
					_printf("cpuinfo max=%.0f, ", cpu.sys.cpuinfo_max_freq);
					_printf("\n");
				}
				if(!(isnan(cpu.sys.scaling_cur_freq) &&
										isnan(cpu.sys.scaling_min_freq) &&
										isnan(cpu.sys.scaling_max_freq))) {
					_printf("\t\t\t");
					_printf("scaling cur=%.0f, ", cpu.sys.scaling_cur_freq);
					_printf("scaling min=%.0f, ", cpu.sys.scaling_min_freq);
					_printf("scaling max=%.0f, ", cpu.sys.scaling_max_freq);
					_printf("\n");
				}
			}
			if(core.cpu_size > 0)
				_printf("\n");
		}
	}
	if(processors_size > 5) {
		_printf("\tomitted remaining processors; total amount: %d\n", processors_size);
	}
	_printf("cpu Hz=%.1f;\n", get_cpu_frequency(-1));
}

/**
 * print information about memory
 */
void print_mem_info() {
	_printf("memory info:\n");
	struct sysinfo info;
	if(sysinfo(&info) == 0) {
		_printf("\tloads 1m=%lu, 5m=%lu, 15m=%lu;\n",
				info.loads[0], info.loads[1], info.loads[2]);
		_printf("\tmemory total=%lu, free=%lu;\n", info.totalram, info.freeram);
		_printf("\tmemory shared=%lu, buffered=%lu;\n", info.sharedram, info.bufferram);
		_printf("\tswap total=%lu, free=%lu;\n", info.totalswap, info.freeswap);
		_printf("\thigh memory total=%lu, free=%lu;\n", info.totalhigh, info.freehigh);
		_printf("\tmemory unit=%d;\n", info.mem_unit);
	}
	else {
		_printf("\tsome error occured while calling sysinfo\n");
	}

}

/**
 * print actually 5 topmost cpu and memory consuming processes
 */
void print_top_info() {
	char buffer[1024];
	FILE *output;
	_printf("process info, sorted by cpu usage (top 5)\n");
	output = popen("ps -eo pid,pcpu,pmem,user,cmd | sort -k 2 -r | head -6", "r");
	while(fgets(buffer, sizeof(buffer)-1, output) != NULL) {
		right_trim(buffer);
		_printf("\t%s\n", buffer);
	}

	_printf("process info, sorted by memory usage (top 5)\n");
	output = popen("ps -eo pid,pcpu,pmem,user,cmd | sort -k 3 -r | head -6", "r");
	while(fgets(buffer, sizeof(buffer)-1, output) != NULL) {
		right_trim(buffer);
		_printf("\t%s\n", buffer);
	}
}

/**
 * print thread affinity, according to thread affinity configuration
 */
#ifdef INTERN
void print_thread_affinity() {
	_printf("\tthread_affinity:");
	if(config.thread_affinity == AFFINITY_NONE) {
		_printf("none;\n");
	}
	else if(config.thread_affinity == AFFINITY_ROUND_ROBIN) {
		_printf("\n\t\t");
		int i;
		for(i=0; i<config.threads->end; i++) {
			_printf("thread%02d = cpu%02d, ", i, get_processorid_recommendation(i));
			if(i%8==7 && i<config.threads->end-2) {
				_printf("\n\t\t");
			}
		}
		_printf("\n");
	}
	else {
		_printf("unknown;\n");
	}
}
#endif

/**
 * print all system information
 */
void print_system_info() {
	time_t t = time(NULL);
	struct tm tm = *localtime(&t);
	_printf("date=%04d-%02d-%02d-%02d:%02d:%02d;\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour, tm.tm_min, tm.tm_sec);

	_printf("system info:\n");
	_printf("\thostname=%s;\n", get_hostname());
#ifdef COMPILE_WITH_MPI
	_printf("\tMPI hosts:\n");
	get_hostname();
	int h;
	for(h=0; h<hostname_size; h++) {
		_printf("\t\thostname%d=%s, cpu model=%s;\n", h, hostname[h], cpuworld[h]);
	}
#endif

	print_cpu_info();
#ifdef INTERN
	print_thread_affinity();
#endif
	print_mem_info();

	print_top_info();

#ifdef COMPILE_WITH_MPI
	print_mpi_info();
#endif
	print_config_info();
	_printf("git revision=%s;\n", GIT_REF);

}

/**
 * print information about MPI
 */
#ifdef COMPILE_WITH_MPI
void print_mpi_info() {
	_printf("\tMPI information:\n");
	// general
#if defined(MPI_VERSION) && defined(MPI_SUBVERSION)
	_printf("\t\tMPI version=%d.%d;\n", MPI_VERSION, MPI_SUBVERSION);
#else
	_printf("\t\tMPI not found (MPI_VERSION is undefined)\n");
	_printf("\t\tMPI version=no MPI;\n");
	return;
#endif
	// vendor specific
	int found_vendor = 0;
#if defined(HP_MPI) && defined(HP_MPI_MINOR)
	found_vendor++;
	_printf("\t\tMPI vendor=%s, version=%d minor %d;\n", "hpmpi", HP_MPI, HP_MPI_MINOR);
#endif
#if defined(MPICH2) && !defined(MPICH2_VERSION)
	found_vendor++;
	_printf("\t\tMPI vendor=%s, version=%d;\n", "mpich2,hpmpi/mpich2,ps5/intel/pgi/mpich2?", MPICH_NAME);
#elif defined(MPICH2) && defined(MPICH2_VERSION)
	found_vendor++;
	_printf("\t\tMPI vendor=%s, version=%s (%d);\n", "mpich2", MPICH2_VERSION, MPICH2_NUMVERSION);
#endif
#if defined(MPICH_NAME) && defined(MPICH_VERSION)
	found_vendor++;
	_printf("\t\tMPI vendor=%s, version=%s;\n", "mpich", MPICH_VERSION);
#endif

#if defined(OPEN_MPI)
	found_vendor++;
	_printf("\t\tMPI vendor=%s, version=%d.%d.%d;\n", "OpenMPI", OMPI_MAJOR_VERSION, OMPI_MINOR_VERSION, OMPI_RELEASE_VERSION);
#endif

	if(found_vendor == 0) {
#ifdef ROMIO_VERSION
		if(ROMIO_VERSION == 124) {
			_printf("\t\tMPI vendor=%s, version=%s;\n", "maybe mpt", "unknown");
		}
		else
#endif
		{ _printf("\t\tMPI vendor=%s, version=%s;\n", "unknown", "unknown"); }
	}
	// probably the most helpful information
#if defined(MPI_INCLUDE_PATH)
	_printf("\t\tMPI include path=%s;\n", MPI_INCLUDE_PATH);
#else
	_printf("\t\tMPI include path=%s;\n", "macro MPI_INCLUDE_PATH missing");
#endif

	int size;
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	_printf("\t\tMPI comm size=%d\n", size);


}
#endif

#define max(a,b) ((a) < (b) ? (b) : (a))
float get_cpu_frequency(int processorid) {
	if(processorid >= cpuinfos_size) return NAN;
	float result = 0;
	processorid = -1; // not providing frequency per processor
	if(processorid == -1) {
		result = max(result, cpuinfos[0].cpuinfo_hz);
		result = max(result, cpuinfos[0].sys.cpuinfo_max_freq);
		result = max(result, cpuinfos[0].sys.scaling_max_freq);
	}
	return result;
}

unsigned get_cpu_count() {
	if(cpuinfos_size == 0) {
		fetch_cpu_info();
	}
	return cpuinfos_size;
}

unsigned get_processorid_recommendation(unsigned i) {
	int rest = i+1;
#ifdef COMPILE_WITH_MPI
	rest += world_rank;
#endif
	// get processor
	if(processors_size == 0) return 0;
	int pid = rest % processors_size;
	rest = (rest / processors_size);
	// get core
	if(processors[pid].processor_size == 0) return 0;
	int cid = rest % processors[pid].processor_size;
	rest = (rest / processors[pid].processor_size) % processors[pid].cores[cid].cpu_size;

	unsigned result = processors[pid].cores[cid].cpuinfos[rest]->processor_id;
	return result;
}
