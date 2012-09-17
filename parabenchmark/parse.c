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
#include "definitions.h"
#include "print_functions.h"
#include "parse.h"
#include "range.h"
#include "system_info.h"

/**
 * trim whitespace of the right side of a string
 */
char *whitespace = " \t\n ";
void right_trim(char *s) {
	int len = strlen(s);
	char *ptr;
	for(ptr = s+len-1; ptr!=s-1; ptr--) {
		char *search_ptr = whitespace;
		bool found = false;
		while(*search_ptr != 0) {
			if(*ptr == *search_ptr) {
				found = true;
				break;
			}
			search_ptr++;
		}
		if(found == false) {
			*(ptr+1) = 0;
			break;
		}
		if(found == true && ptr == s)
			*(ptr) = 0;
	}
}

/**
 * Get token separated by comma, and options specified by []
 */
char* get_token(get_token_t *interna, char *string, char **option) {
	char delimiter = ',';
	char option_start = '[', option_end = ']';
	char *stringindex;
	if(option != NULL) *option = NULL;

	if(string != interna->source_address || string == NULL){
		free(interna->working_copy);
		if(string == NULL) return NULL;

		unsigned length = strlen(string);
		interna->working_copy = malloc(sizeof(char)*(length+1));
		strcpy(interna->working_copy, string);
		interna->working_pointer = interna->working_copy;
		interna->source_address = string;
	}

	char *result = interna->working_pointer;

	bool read_option = false;
	stringindex = interna->working_pointer;
	int nested = 0;
	while(*stringindex != '\0') {

		//printf("char: %c\n", *stringindex);
		if(!read_option && *stringindex == delimiter) {
			*stringindex = '\0';
			result = interna->working_pointer;
			stringindex++;
			break;
		}
		if(option != NULL && !read_option && *stringindex == option_start) {
			*stringindex = '\0';
			*option = stringindex+1;
			result = interna->working_pointer;
			interna->working_pointer = stringindex+1;
			read_option = true;
		}
		if(option != NULL && read_option) {
			if(*stringindex == option_end && nested == 0) {
				*stringindex = '\0';
				stringindex++;
				if(*stringindex == delimiter) stringindex++;
				break;
			}
			if(*stringindex == option_end && nested > 0) {
				nested--;
				if(nested < 0) {
					_printf("WARNING: some strange error in function get_token ");
					_printf("while processing \"%s\"\n", interna->working_pointer);
				}
			}
			if(*stringindex == option_start) {
				nested++;
			}
		}
		stringindex++;
	}

	if(stringindex == interna->working_pointer)
		return NULL;

	interna->working_pointer = stringindex;
	return result;
}

/**
 * Get token separated by comma, as an array
 */
char **get_token_array(char *str, unsigned *size) {
	*size = 0;
	char **result = NULL;
	get_token_t token_interna = GET_TOKEN_T_INIT;
	char *token;
	while((token = get_token(&token_interna, str, NULL)) != NULL) {
		(*size)++;
		char **new = (char**)realloc(result, *size*sizeof(char*));
		if(new == NULL) break;
		else result = new;
		right_trim(token);
		while(*token != '\0') {
			if(*token == ' ' || *token == '\t' || *token == '\n') token++;
			else break;
		}
		result[*size-1] = token;
	}
	return result;
}

/**
 * parse 'true' or 'false' to boolean value, or else use default value 'nooptarg'
 */
int parse_bool_option(char *name, char *optarg, bool *var, bool nooptarg) {
	if(optarg != NULL) {
		if(strcmp(optarg, "true")==0) {
			*var = true;
		}
		else if (strcmp(optarg, "false")==0) {
			*var = false;
		}
		else {
			_printf("not recognized output-to-files option: %s, setting to %s\n", optarg, BOOL_STR(nooptarg));
			*var = nooptarg;
			return -1;
		}
	}
	else {
		_printf("not recognized output-to-files option: %s, setting to %s\n", optarg, BOOL_STR(nooptarg));
		*var = nooptarg;
	}
	return 0;
}

/**
 * Parse range, specified as comma-seperated list of a-b, possibly with option [option]
 *
 * Possible options are
 *    +c      increment by c
 *    *d      multiply by d
 *    xn*d    segment factor d logarithmically (n times)
 *
 * Defaults:
 *    a-b     exponential increase
 *    a-b[]   increment by 1
 *
 * Examples:
 *    3-20          3,6,12
 *    3-5[]         3,4,5
 *    3-33[+10]     3,13,23,33
 *    1-1000[*10]   1,10,100,1000
 *    8-64[*1.4142135623*2]   8,11,16,22,32,45,64
 *    8-64[x2*2]    8,11,16,22,32,45,64
 *    10-1000[*2.5*5*7.5*10]  10,25,50,75,100,250,500,750,1000
 *    10-40,2-5[]   10,20,40,2,3,4,5
 *    1-4[],2-5[]   1,2,3,4,2,3,4,5
 */
range_t *parse_range_option(char *opt) {
	get_token_t token_interna = GET_TOKEN_T_INIT;
	range_t init_value = RANGE_T_INIT;
	char *option, *token;
	range_t *result = NULL;
	while((token = get_token(&token_interna, opt, &option)) != NULL) {
		// initializing data structure
		range_t *act;
		range_t *new = (range_t*) malloc(sizeof(range_t));
		if(result == NULL) {
			result = new;
			act = new;
		}
		else {
			act->next_range = new;
			act = new;
		}
		*act = init_value;
		if(option == NULL)  {
			range_add_factor(act, 2.0);
		}

		// parsing token
		unsigned_huge start, end;
		int separator;
		char *format = "%Lu-%n%Lu"; //"%i%*[\\-]%n%i[%n%d]%n";

		start = end = separator = 0;
		sscanf(token, format, &start, &separator, &end);
		act->start = start;
		if(separator != 0) {
			act->end = end;
		}
		else {
			act->end = start;
		}

		// parsing option
		get_token_t token_option_interna = GET_TOKEN_T_INIT;
		char *opt_opt, *opt_tok;
		;
		unsigned dense_log_factors = 0;
		while((opt_tok = get_token(&token_option_interna, option, &opt_opt))!=NULL) {
			if(strcmp(opt_tok, "*")==0) {
				double factor = atof(opt_opt);
				if(dense_log_factors == 0) {
					range_add_factor(act, factor);
				}
				else {
					double root = pow(factor, ((double)1.0) / dense_log_factors);
					int i;
					for(i=0; i<dense_log_factors; i++) {
						range_add_factor(act, pow(root, i+1));
					}
				}
			}
			else if(strcmp(opt_tok, "+")==0) {
				act->addition = atoi(opt_opt);
			}
			else if(strcmp(opt_tok, "*+")==0) {
				act->factors.after_addition = atoi(opt_opt);
			}
			else if(strcmp(opt_tok, "x") == 0) {
				dense_log_factors = atoi(opt_opt);
			}
			else {
				while(*opt_tok != '\0') {
					if(opt_tok[0] == '*' && opt_tok[1] == '+') {
						opt_tok += 2;
						int val;
						opt_tok += sscanf(opt_tok,"%30d", &val);
						act->factors.after_addition = val;
					}
					else if(opt_tok[0] == '*') {
						opt_tok++;
						float val;
						opt_tok += sscanf(opt_tok,"%30f", &val);
						double factor = val;
						if(dense_log_factors == 0) {
							range_add_factor(act, factor);
						}
						else {
							double root = pow(factor, ((double)1.0) / dense_log_factors);
							int i;
							for(i=0; i<dense_log_factors; i++) {
								range_add_factor(act, pow(root, i+1));
							}
						}
					}
					else if(opt_tok[0] == 'x') {
						opt_tok++;
						unsigned val;
						opt_tok += sscanf(opt_tok,"%30u", &val);
						dense_log_factors = val;
					}
					else if(opt_tok[0] == '+') {
						opt_tok++;
						int val;
						opt_tok += sscanf(opt_tok,"%30d", &val);
						act->addition = val;
					}
					else {
						opt_tok++;
					}
				}
			}
		}
	}
	get_token(&token_interna, NULL,NULL);
	return result;
}

