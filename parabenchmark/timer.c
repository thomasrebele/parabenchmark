/*
 * timer.c
 *
 * Helper functions for measuring time
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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <math.h>
#include <sched.h>
#include "timer.h"
#include "statistics.h"

#ifdef COMPILE_WITH_MPI
#include <mpi.h>
#endif

statistic_t calibration_tick_stat = STATISTIC_T_INIT;
statistic_t context_switch_stat = STATISTIC_T_INIT;

const byte MODE_START = 0;
const byte MODE_END = 1;

timespec_t timespec_diff(timespec_t, timespec_t);
timespec_t timer_clock_gettime_end();
void timer_clock_gettime_start();

struct timespec diff;
unsigned_huge last_cs_count;

/**
 * get count of context switches
 */
unsigned context_switch_count() {
    struct rusage usage;
    int err;
    if ((err = getrusage(RUSAGE_SELF, &usage)) != 0) {
        return 0;
    }
    return usage.ru_nvcsw + usage.ru_nivcsw;
}

/**
 * Tries to get mean time of a context switch.
 * Isn't used, as the deviation is very high
 */
void calibrate_context_switch_stat() {
	double act_mean = 0;
	if(context_switch_stat.sample_size > 0) act_mean = context_switch_stat.mean;
	statistic_t stat = STATISTIC_T_INIT;
	int i = 0;
	unsigned cs, cs2;
	double time;
	while (i < 500) {
		tick(MODE_START);
		cs = context_switch_count();
		sched_yield();
		if((cs2 = context_switch_count()) != cs) {
			time = tick(MODE_END);
			unsigned diff = cs2 - cs;
			double time_per_cs = act_mean + time / diff;
			calculate_statistics_iterative(&stat, time_per_cs);
			i++;
		}
	}
	context_switch_stat = stat;
	_printf("context switch mean=%f, ", context_switch_stat.mean);
	_printf("deviation=%f;\n", context_switch_stat.deviation);
}

/**
 * Time measurement is started with tick(MODE_START)
 * and ended with tick(MODE_END) which returns time
 */
double tick(byte modus){
	if(modus == MODE_START){
		last_cs_count = context_switch_count();
		timer_clock_gettime_start();
	}
	else if(modus == MODE_END){
		diff = timer_clock_gettime_end();
		double time = (double) diff.tv_sec 
			+ (double) diff.tv_nsec / (double) 1000000000;
		if(isnan(calibration_tick_stat.mean))
			return time;
		else {
			/*unsigned diff = context_switch_count() - last_cs_count;
			double cs_time = 0; /* isnan(diff * context_switch_stat.mean) ?
					0 : diff * context_switch_stat.mean;*/
			return time - calibration_tick_stat.mean /*- cs_time*/;
		}
	}
	return 0;
}

/**
 * allows more than one time measurement at the same time
 */
double tick2(byte modus, double *tmp){
	timespec_t act;
	timespec_t last_time;
	timespec_t zero; zero.tv_nsec = 0; zero.tv_sec = 0;
	clock_gettime(CLOCK_REALTIME , &last_time);
	act = timespec_diff(zero, last_time);
	double act_time = (double) act.tv_sec
			+ (double) act.tv_nsec / (double) 1000000000;

	if(modus == MODE_START){
		*tmp = act_time;
	}
	else if(modus == MODE_END){
		return act_time - *tmp;
	}
	return 0;
}

/**
 * calibrate timer (the time needed for the tick start and end call is measured and subtracted)
 */
void calibrate_timer() {
	_printf("calbrating timer ...");
	fflush(stdout);
	statistic_t stat = STATISTIC_T_INIT;
	calibration_tick_stat.mean = 0;
	int r;
	for(r=0; r<100000; r++) {
		tick(MODE_START);
		double time = tick(MODE_END);
		calculate_statistics_iterative(&stat, time);
		if(stat.mean * stat.sample_size > 0.1) break;
	}
	calibration_tick_stat = stat;
	_printf(" finished, ");
	_printf("mean=%"PRECISSION "f, ", calibration_tick_stat.mean);
	_printf("deviation=%" PRECISSION "f;\n", calibration_tick_stat.deviation);
	/*calibrate_context_switch_stat();*/
}

/**
 * timer for MPI with implicit barrier
 */
#ifdef COMPILE_WITH_MPI
double last_time_double;
double tick_mpi(byte modus, MPI_Comm barrier){
	if(barrier != MPI_COMM_NULL) {
		MPI_Barrier(barrier);
	}
	if(modus == MODE_START){
		timer_clock_gettime_start();
		return 0;
	}
	else if(modus == MODE_END){
		diff = timer_clock_gettime_end();
		double time = (double) diff.tv_sec
			+ (double) diff.tv_nsec / (double) 1000000000;
		return time;
	}
	if(barrier != MPI_COMM_NULL) {
		MPI_Barrier(barrier);
	}

	return 0;
}
#endif

// clock_gettime

timespec_t last_time;
void timer_clock_gettime_start() {
	//clock_gettime(CLOCK_MONOTONIC, &last_time);
	clock_gettime(CLOCK_REALTIME , &last_time);
}

timespec_t timer_clock_gettime_end() {
	timespec_t act_time;
	//clock_gettime(CLOCK_MONOTONIC, &act_time);
	clock_gettime(CLOCK_REALTIME , &act_time);
	
	timespec_t diff = timespec_diff(last_time, act_time);

	return diff;
}

timespec_t timespec_diff(timespec_t start, timespec_t end){
	timespec_t result;
	result.tv_sec = end.tv_sec - start.tv_sec;
	result.tv_nsec = end.tv_nsec - start.tv_nsec;
	
	if(result.tv_nsec < 0){
		result.tv_sec -= 1;
		result.tv_nsec += 1000000000;
	}

	return result;
}
