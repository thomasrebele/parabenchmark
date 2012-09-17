/*
 * statistics.c
 *
 * Calculate the mean and deviation
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
#include "statistics.h"

/**
 * calculate mean and deviation using the method described in
 * http://www.cs.berkeley.edu/~mhoemmen/cs194/Tutorials/variance.pdf
 */
void calculate_statistics_iterative(statistic_t *stat, double value) {
	int r = stat->sample_size+1;
	if(r == 1) {
		stat->_m = value;
		stat->_q = 0;
		stat->mean = value;
		stat->deviation = NAN;
		stat->error = NAN;
	}
	else {
		stat->_q += (r-1)*(value - stat->mean)*(value - stat->mean)/(r);
		stat->_m += (value - stat->mean)/(r);
		stat->mean = stat->_m;
		stat->deviation = sqrt(stat->_q / (r-1));
		stat->error = stat->deviation / sqrt(r);
	}
	stat->sample_size++;
}

/**
 * Obsolete.
 * Calculate statistic using a double array (or array of other data type containing double)
 *
 * 	f: function to access next double value;
		arguments: f_arg, address of array, index of actual element
	f_arg: way to give f information (how to access double in struct for example)
	start: address of array
	length: length of array
 */
statistic_t calculate_statistics(
		double (*f)(void *, void *, int),
		void *f_arg,
		void *start,
		int length)
{
	// calculating mean / std deviation
	double mean = 0;
	double deviation = 0;
	double percent = 0;
	double value;
	int r;
	for(r=1; r<=length; r += 1) {
		value = f(f_arg, start, r-1);
		if(r==1) {
			mean = value;
		}
		else {
			deviation += (r-1)*(value - mean)*(value - mean)/(r);
			mean += (value - mean)/(r);
		}
	}
	if(length > 1) {
		deviation /= (length-1);
		deviation = sqrt(deviation);

		percent = deviation / mean * 100;
	}
	else {
		deviation = percent = NAN;
	}

	statistic_t result;
	result.mean = mean;
	result.deviation = deviation;
	result.sample_size = length;
	return result;
}

/**
 * Obsolete.
 * Access a certain double of a struct
 *
 * args_void: how to access the double value in struct (struct length, offset)
 * start_void: address of struct array
 * index: which element of array should be accessed
 */
double struct_access(void *args_void, void *start_void, int index) {
	struct_access_args_t *args = (struct_access_args_t*) args_void;
	char *start = (char *) start_void;

	size_t length = args->length;
	size_t offset = args->offset;

	double value = *((double *) (start + index*length + offset));
	return value;
}

/**
 * Obsolete.
 * Access a double in a double array
 */
double direct_double_access(void *args_void, void *start_void, int index) {
	double *start = (double *) start_void;
	double value = *(start + index);
	return value;
}
