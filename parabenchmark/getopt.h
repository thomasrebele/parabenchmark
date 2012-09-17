/*
 * getopt.h
 *
 * This file provides a struct for saving parabenchmark's command
 * line interface configurations.
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

#ifndef __GETOPT_H
#define __GETOPT_H

#include "definitions.h"
#include <regex.h>
#include <getopt.h>

typedef struct option_info_t_ {
	int option_val;
	const char *description;

	const char *name;
	char *argument_info;

	int has_arg;
	int *flag;
	bool print_space;
} option_info_t;

//void *opt_thread_dest[] = ;
typedef struct opt_arg_t_ {
	int option;
	char *format;
	void *destination[10];
} opt_arg_t;

struct option* getopt_init(option_info_t *infos, char** short_options);
void print_usage(char *executable);

#endif
