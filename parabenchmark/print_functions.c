/*
 * print_functions.c
 *
 * Helper functions for printing to file or stdout; printing tables with
 * (non-aligned) header.
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

#include <unistd.h>
#include "print_functions.h"

#ifdef INTERN
#include "config.h"
extern config_t config;
#endif
#ifdef COMPILE_WITH_MPI
#include "mpi_functions.h"
extern int world_rank;
#endif

int (*postprocess_vprintf)(const char *, va_list args) = &vprintf;

int (*stdout_vprintf)(const char *, va_list args) = &vprintf;
int (*file_vprintf)(FILE *, const char *, va_list args) = &vfprintf;
FILE *output_file = NULL;

bool first = true;

/**
 * Add number sign (#) to every line and print using postprocess_vprintf
 */
int printf_function(const char *fmt, ...) {
	if(fmt == NULL) return 0;

	int chars;
	char *format = (char *)malloc((3*strlen(fmt)+4)*sizeof(char));
	const char *fmtptr = fmt;
	char *formatptr = format;
	if(first == true) {
		*formatptr++ = '#';
		*formatptr++ = ' ';
		first = false;
	}
	while(*fmtptr != 0) {
		*formatptr++ = *fmtptr;
		if(*fmtptr == '\n') {
			if(*(fmtptr+1) == 0) {
				first = true;
			}
			else {
				*formatptr++ = '#';
				*formatptr++ = ' ';
			}
		}
		*fmtptr++;
	}
	*formatptr = 0;

	va_list args;
	va_start(args, fmt);
	chars = postprocess_vprintf(format, args);
	va_end(args);
	free(format);
	return chars;
}
int (*_printf)(const char *, ...) = &printf_function;

/**
 * print directly with postprocess_vprintf
 */
int _printf_function(const char *fmt, ...) {
	int chars;
	va_list args;
	va_start(args, fmt);
	chars = postprocess_vprintf(fmt, args);
	va_end(args);
	return chars;
}

/**
 * flush output to stdout / file
 */
void flush_output() {
	fflush(stdout);
#ifdef COMPILE_WITH_MPI
	if(world_rank != 0) return;
#endif
	if(output_file != NULL) {
		fflush(output_file);
	}
}

/**
 * print to file and/or stdout (depending on output tee)
 */
int vprintf_to_file(const char *format, va_list args) {
#ifdef COMPILE_WITH_MPI
	if(world_rank != 0) return 0;
#endif
	if(args == NULL) return 0;
	int characters;
	if(output_file == NULL) {
		characters = vprintf(format, args);
	}
	else {
#ifdef INTERN
		if(config.output_tee) {
			va_list copy;
			va_copy(copy, args);
			stdout_vprintf(format, copy);
			va_end(copy);
		}
#endif
		characters = vfprintf(output_file, format, args);
	}
	return characters;
}

/**
 * set postprocess_vprintf
 */
void set_vprintf_function(int (*f)(const char*, va_list args)) {
	postprocess_vprintf = f;
	stdout_vprintf = f;
}

/**
 * set output to file
 */
void set_output_to_file(const char *filename, const char* mode) {
	if(output_file != NULL) {
		fclose(output_file);
		output_file = NULL;
	}
	if(filename == NULL) {
		postprocess_vprintf = stdout_vprintf;
		return;
	}

	char *path = NULL;
#ifdef INTERN
	strappend(&path, config.output_dir);
#endif
	strappend(&path, "/");
	strappend(&path, filename);
	printf("# try to open file %s\n", path);
	output_file = fopen(path, mode);
	if(output_file == NULL) {
		_printf("ERROR while opening file %s\n", path);
	}
	else {
		_printf("opened output file %s\n", path);
		postprocess_vprintf = &vprintf_to_file;
	}
	free(path);
}

/**
 * set output to stdout
 */
void set_output_to_stdout() {
	postprocess_vprintf = stdout_vprintf;
}

// ----------------------------------------------------------------------------
/**
 * print number of bytes with suffix B/KB/MB/GB
 */
void print_num_bytes(unsigned_huge length) {
	if(length >= GB) {
		_printf("%6.1fGiB", (double)length/GB);
	}
	else if(length > MB) {
		_printf("%6.1fMiB", (double)length/MB);
	}
	else if(length > KB) {
		_printf("%6.1fKiB", (double)length/KB);
	}
	else {
		_printf("%8ldB", length);
	}
}

/**
 * print number of bytes with suffix B/KB/MB/GB to string
 */
char *sprint_num_bytes(unsigned_huge length) {
	char *buffer = malloc(sizeof(char)*100);
	if(length >= GB) {
		sprintf(buffer, "%6.1fGiB", (double)length/GB);
	}
	else if(length >= MB) {
		sprintf(buffer, "%6.1fMiB", (double)length/MB);
	}
	else if(length >= KB) {
		sprintf(buffer, "%6.1fKiB", (double)length/KB);
	}
	else {
		sprintf(buffer, "%8ldB", length);
	}
	return buffer;
}

/**
 * table printing functions
 */
int max_header_num = 100;
char *header_name[100];
unsigned header_name_index = 0;
char *intern_additional_info_header = NULL;
char *intern_additional_info = NULL;

/**
 * concatenate strings (reallocates if necessary)
 */
void strappend(char **str, const char *append) {
	if(str == NULL) {
		_printf("Error: strappend cannot append to empty string\n");
		return;
	}
	if(append == NULL) {
		return;
	}
	unsigned append_length = strlen(append);
	if(*str == NULL) {
		*str = malloc(sizeof(char)*append_length+1);
		strcpy(*str, append);
	}
	else {
		unsigned str_length = strlen(*str);
		*str = realloc(*str, (sizeof(char)*(str_length+append_length+10)));
		strcpy(*str+str_length, append);
	}
}

/**
 * set additional info
 * (additional info is printed on each table row)
 */
void print_table_set_additional_info(
		char *additional_info_header,
		char *additional_info) {

	if(additional_info_header == NULL || additional_info == NULL) {
		free(intern_additional_info_header);
		free(intern_additional_info);
		intern_additional_info_header = NULL;
		intern_additional_info = NULL;
	}

	intern_additional_info_header = additional_info_header;
	intern_additional_info = additional_info;
}

/**
 * append additional info to already existing info
 */
void print_table_add_additional_info(
		char *additional_info_header,
		char *additional_info) {

	if(additional_info_header == NULL || additional_info == NULL) {
		free(intern_additional_info_header);
		free(intern_additional_info);
		intern_additional_info_header = NULL;
		intern_additional_info = NULL;
	}

	strappend(&intern_additional_info_header, additional_info_header);
	strappend(&intern_additional_info, additional_info);
}

char* first_line = NULL;
char* first_line_next_position;

bool intern_print_header = false;
bool first_header = true;

/**
 * trigger header print with next line
 */
void print_header() {
	intern_print_header = true;
}

/**
 * Print one cell of table. If a header should be printed, the line is cached,
 * until print_table_line is called. Format is the standard printf format,
 * except, that you can specify the header names after the percent sign enclosed
 * in curly braces, like "%{datasize}15d"
 *
 */
void print_table_cell(char* format, ...) {

	// parsing format argument
	// determine buffer size
	unsigned size = 0, max_header_num = 0;
	char *formatindex = format;
	while(*formatindex++ != '\0') {
		size++;
		if(*formatindex == '%')
			max_header_num++;
	}
	formatindex = format;
	char *new_format = malloc(sizeof(char)*(size+1));
	char *buffer_index = new_format;

	char* format_header_index;
	int header_column[max_header_num];
	while(*formatindex != '\0') {
		while(*formatindex != '\0' && *formatindex != '%') {
			*buffer_index++ = *formatindex++;
		}

		// enable special processing of %
		if(*formatindex == '%') {
			*buffer_index++ = '%';
			switch(*(++formatindex)) {
			case '%':
				*buffer_index++ = '%';
				*buffer_index++ = '%';
				break;
			case '{': {
				// determine size of table header name
				char *header_index = ++formatindex;
				size = 0;
				while(*header_index++ != '}') {
					size++;
					if(*header_index == '\0') {
						intern_print_header = false;
						_printf("Error: missing '}' in argument of print_table_line: %s\n", format);
						return;
					}
				}

				// check if header should be printed
				if(intern_print_header) {
					// save header name in header_name_array
					char *name = (char *)malloc(sizeof(char)*(size+1));
					int i;
					for(i=0; i<size; i++) {
						name[i] = formatindex[i];
					}
					name[i] = '\0';
					header_name[header_name_index] = name;
					//printf("found header '%s', char %c\n", header_name[header_name_index], *header_index);

					header_name_index++;
					formatindex = header_index;
				}
				else {
					formatindex = header_index;
				}
			}
				break;
			default:
				*buffer_index++ = *formatindex;
				break;
			}
			//formatindex++;
		}
	}
	*buffer_index = '\0';

	va_list vargs;
	va_start(vargs, format);
	if(intern_print_header) {
		if(first_line == NULL) {
			first_line = malloc(sizeof(char)*10000);
			first_line_next_position = first_line;
		}

		first_line_next_position += vsprintf(first_line_next_position, new_format, vargs);
	}
	else {
		postprocess_vprintf(new_format, vargs);
	}
	va_end(vargs);

	free(new_format);
	fflush(stdout);
}

/**
 * print table line and \n, preceded by header if print_header was called before
 */
void print_table_line() {
	if(intern_print_header) {
		intern_print_header = false;
		if(!first_header) {
			_printf_function("\n# ");
		}
		else {
			first_header = false;
		}
		int i;
		for(i=0; i<header_name_index; i++) {
			if(header_name != NULL) {
				_printf_function("%s, ", header_name[i]);
				free(header_name[i]);
				header_name[i] = NULL;
			}
		}
		_printf_function(intern_additional_info_header);
		_printf_function("\n");
		header_name_index = 0;

		if(first_line != NULL) {
			_printf_function("%s", first_line);
			free(first_line);
			first_line = NULL;
		}
	}
	_printf_function(intern_additional_info);
	_printf_function("\n");

	if(output_file != NULL) {
		fflush(stdout);
		fflush(output_file);
		fsync(fileno(output_file));
	}

}
