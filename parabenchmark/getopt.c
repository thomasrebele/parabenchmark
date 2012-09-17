/*
 * getopt.c
 *
 * This helper functions generate the usage information for --help option
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

#include "getopt.h"
#include "print_functions.h"

struct option_info_t_ *option_infos; // is defined in main.c

/**
 * Print the usage information saved in array 'option_infos'
 */
void print_usage(char *executable) {
	_printf("usage: \n");
	_printf("\t%s [options]\n\n", executable);

	_printf("options:\n");
	struct option_info_t_ *info_pointer = (struct option_info_t_*)&option_infos;

	while(true) {
		struct option_info_t_ item = *info_pointer;

		if(item.name == NULL)
			break;

		const char *description = "";
		if(item.description != NULL)
			description = item.description;

		char option_usage[1024];
		const char *argument = "arg";
		if(item.argument_info != NULL)
			argument = item.argument_info;
		if(item.has_arg == required_argument) {
			sprintf(option_usage, "%s %s", item.name, argument);
		}
		else if(item.has_arg == optional_argument) {
			sprintf(option_usage, "%s[=%s]", item.name, argument);
		}
		else {
			sprintf(option_usage, "%s", item.name);
		}

		_printf("\t");
		if(item.option_val < 255) {
			char short_opt[10] = "-  | ";
			short_opt[1] = item.option_val;
			short_opt[6] = '\0';
			_printf("%s", short_opt);
		}

		_printf("--%-40s", option_usage);
		_printf("%-40s\n", description);

		if(item.print_space) {
			_printf("\n");
		}

		info_pointer++;
	}
}

/**
 * For debugging
 */
void print_long_option_vector(struct option *long_options) {
	while (true) {
		if(long_options->name == NULL) {
			break;
		}

		_printf("\n");
		_printf("\tname: %s\n", long_options->name);
		_printf("\thas_arg: %d\n", long_options->has_arg);
		_printf("\tflag: %p\n", long_options->flag);
		_printf("\tval: %d\n", long_options->val);
		_printf("---\n");

		long_options++;
	}

	_printf("\n");
	_printf("\tname: %s\n", long_options->name);
	_printf("\thas_arg: %d\n", long_options->has_arg);
	_printf("\tflag: %p\n", long_options->flag);
	_printf("\tval: %d\n", long_options->val);
}

/**
 * Process a 'struct option_info_t *' to a 'struct option *'
 */
struct option* getopt_init(struct option_info_t_ *infos, char** short_options) {
	int size = 0;
	int short_option_size = 1;
	struct option_info_t_ *info_pointer = infos;

	while(true) {
		if(info_pointer->name == NULL) {
			break;
		}
		if(info_pointer->option_val < 255) {
			short_option_size++;
			short_option_size += info_pointer->has_arg;
		}
		size++;
		info_pointer++;
	}

	int result_bytes_length = sizeof(struct option) * (size+1);
	struct option *result = (struct option *)malloc(result_bytes_length);
	char *short_opt = (char*) malloc(sizeof(char)*(short_option_size+1));
	if(short_opt == NULL) {
		_printf("error: cannot allocate %s bytes!\n", short_option_size);
	}

	int i;
	short_opt[0] = '0';
	int short_opt_idx = 1;
	for(i=0; i<size; i++) {
		struct option *act = &result[i];
		result[i].val = infos[i].option_val;
		result[i].name = infos[i].name;

		result[i].has_arg = infos[i].has_arg;
		result[i].flag = infos[i].flag;

		if(result[i].val < 255) {
			short_opt[short_opt_idx++] = result[i].val;

			if(infos[i].has_arg == required_argument) {
				short_opt[short_opt_idx++] = ':';
			}
			else if(infos[i].has_arg == optional_argument) {
				short_opt[short_opt_idx++] = ':';
				short_opt[short_opt_idx++] = ':';
			}
		}
	}
	short_opt[short_opt_idx] = '\0';
	short_options[0] = short_opt;

	result[size].flag = NULL;
	result[size].has_arg = 0;
	result[size].name = NULL;
	result[size].val = 0;

	return result;
}

