
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "timer.h"

const byte MODE_START = 0;
const byte MODE_END = 1;

timespec timespec_diff(timespec, timespec);
timespec timer_clock_gettime_end();
void timer_clock_gettime_start();

struct timespec diff;
double tick(byte modus){
	if(modus == MODE_START){
		timer_clock_gettime_start();
	}
	else if(modus == MODE_END){
		diff = timer_clock_gettime_end();
		//printf("%d.%.9d\n", diff.tv_sec, diff.tv_nsec);
		double time = (double) diff.tv_sec 
			+ (double) diff.tv_nsec / (double) 1000000000;
		return time;
	}
	return 0;
}


// clock_gettime

timespec last_time;
void timer_clock_gettime_start() {
	//clock_gettime(CLOCK_MONOTONIC, &last_time);
	clock_gettime(CLOCK_REALTIME , &last_time);
}

timespec timer_clock_gettime_end() {
	timespec act_time;
	//clock_gettime(CLOCK_MONOTONIC, &act_time);
	clock_gettime(CLOCK_REALTIME , &act_time);
	
	timespec diff = timespec_diff(last_time, act_time);

	return diff;
}

timespec timespec_diff(timespec start, timespec end){
	timespec result;
	result.tv_sec = end.tv_sec - start.tv_sec;
	result.tv_nsec = end.tv_nsec - start.tv_nsec;
	
	if(result.tv_nsec < 0){
		result.tv_sec -= 1;
		result.tv_nsec += 1000000000;
	}

	return result;
}
