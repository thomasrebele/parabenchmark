/*
 * parse.c
 *
 * Helper functions for parsing command line arguments.
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

#ifndef __PARSE_H
#define __PARSE_H

#include "range.h"

typedef struct get_token_t_ {
	char* source_address;
	char* working_copy;
	char* working_pointer;
} get_token_t;
#define GET_TOKEN_T_INIT { NULL, NULL, NULL}

char* get_token(get_token_t *pointers, char *string, char **option);
char **get_token_array(char *str, unsigned *size);

void right_trim(char *s);

int parse_bool_option(char *name, char *optarg, bool *var, bool nooptarg);
range_t *parse_range_option(char *opt);
#endif
