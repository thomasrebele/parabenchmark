#ifndef __PRINT_FUNCTIONS_H
#define __PRINT_FUNCTIONS_H

#define INTERN
#ifdef EXTERN
#undef INTERN
#endif

#include "definitions.h"
#ifdef INTERN
#include "config.h"
#endif

void set_output_to_file(const char *filename, const char* mode);
void set_vprintf_function(int (*f)(const char*, va_list args));
int printf_function(const char *fmt, ...);
void print_num_bytes(unsigned_huge length);
void print_header();
void print_table_line();
void print_table_cell(char* format, ...);

char *sprint_num_bytes(unsigned_huge length);

void print_table_add_additional_info(
		char *additional_info_header,
		char *additional_info);

void print_table_set_additional_info(
		char *additional_info_header,
		char *additional_info);

void strappend(char **str, const char *append);

static const byte MODE_PLOT = 1;
static const byte MODE_HUMAN = 2;

#define BOOL_STR(var) (var ? "true" : "false")

#endif
