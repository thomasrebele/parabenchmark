/*
 * print_functions.h
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

#ifndef __PTHREAD_FUNCTIONS_H
#define __PTHREAD_FUNCTIONS_H

typedef enum {
	THREAD_CREATED,
	THREAD_INITIALIZED,
	THREAD_RUNNING,
	THREAD_CANCEL,
	THREAD_FINISHED
} thread_status_t;

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t condition;
	int tid;
} thread_cond_t;

#define THREAD_COND_T_INIT {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, -1}

struct test_function_arg_t_;
typedef struct test_function_arg_t_ thread_arg_t;
struct test_function_arg_t_ {
	pthread_t *thread;
	unsigned tid;
	unsigned thread_count;
	thread_arg_t *allargs;

	void *(*loop_function)(void *);
	unsigned_huge iteration_start;
	unsigned_huge iteration_end;

	thread_cond_t init_cond;
	thread_cond_t start_cond;
	thread_cond_t end_cond;
	volatile thread_status_t status;

	thread_cond_t reduce_cond;
	bool reduce_finished;
	bool reduce;
	huge result;
	double time;
};

#define THREAD_ARG_T_INIT { \
		NULL, 0, 0, NULL, \
		NULL, 0, 0, \
		THREAD_COND_T_INIT, THREAD_COND_T_INIT, THREAD_COND_T_INIT, THREAD_CREATED, \
		THREAD_COND_T_INIT, false, false, 0, 0.0}

thread_arg_t *get_thread_array(unsigned num);
void thread_init_wait(thread_arg_t *arg);
void *thread_function(void *arg);

void thread_affinity(int threadid);
void reduce_plus(thread_arg_t *args);

#endif
