
#include "definitions.h"
#include "pthread_functions.h"
#include "config.h"
#include "system_info.h"

#include <unistd.h>
#define __USE_GNU
#include <sched.h>
#include <pthread.h>
#include <limits.h>

pthread_t *threads = NULL;
thread_arg_t *thread_arguments = NULL;
unsigned threads_size = 0;


void thread_cond_destroy(thread_cond_t *cond) {
	pthread_mutex_destroy(&cond->mutex);
	pthread_cond_destroy(&cond->condition);
}

void thread_cond_signal(thread_cond_t *cond, bool *var) {
	pthread_mutex_lock(&cond->mutex);
	if(var != NULL) {
		*var = true;
	}
	pthread_cond_signal(&cond->condition);
	pthread_mutex_unlock(&cond->mutex);
}

void thread_cond_wait(thread_cond_t *cond, bool *var) {
	pthread_mutex_lock(&cond->mutex);
	if(!*var) {
		pthread_cond_wait(&cond->condition, &cond->mutex);
		*var = false;
	}
	pthread_mutex_unlock(&cond->mutex);
}

void thread_init_signal(thread_arg_t *arg) {
	pthread_mutex_lock(&arg->init_cond.mutex);
	arg->status = THREAD_INITIALIZED;
	pthread_cond_signal(&arg->init_cond.condition);
	pthread_mutex_unlock(&arg->init_cond.mutex);
}

void thread_init_wait(thread_arg_t *arg) {
	pthread_mutex_lock(&arg->init_cond.mutex);
	if(arg->status != THREAD_INITIALIZED) {
		pthread_cond_wait(&arg->init_cond.condition, &arg->init_cond.mutex);
	}
	pthread_mutex_unlock(&arg->init_cond.mutex);
}

void thread_end_wait(thread_arg_t *arg) {
	pthread_mutex_lock(&arg->end_cond.mutex);
	if(arg->status == THREAD_RUNNING) {
		pthread_cond_wait(&arg->init_cond.condition, &arg->init_cond.mutex);
	}
	pthread_mutex_unlock(&arg->init_cond.mutex);
}

inline void reduce_plus(thread_arg_t *args) {
	inline void wait_partner(thread_arg_t *partner) {
		thread_cond_wait(&partner->reduce_cond, &partner->reduce_finished);
	}

	int reduce_counter = 1;
	int reduce_tid = args->tid;
	int reduce_thread_count = args->thread_count;
	int reduce_index = 0;
	unsigned reduce_partner_id;
	// compine partial results
	while(true) {
		if(reduce_tid % 2 == 1 ||
				reduce_tid == reduce_thread_count -1) {
			thread_cond_signal(&args->reduce_cond, &args->reduce_finished);
			break;
		}
		else {
			reduce_partner_id = args->tid + reduce_counter;
			//printf("%d: thread %03d joining %03d\n", reduce_index, args->tid, reduce_partner_id);
			wait_partner(&args->allargs[reduce_partner_id]);
			args->result += args->allargs[reduce_partner_id].result;

			if(reduce_tid + 3 == reduce_thread_count) {
				reduce_partner_id += reduce_counter;
				//printf("%d: thread %03d joining %03d (last)\n", reduce_index, args->tid, reduce_partner_id);
				wait_partner(&args->allargs[reduce_partner_id]);
				args->result += args->allargs[reduce_partner_id].result;
			}

			reduce_counter *= 2;
			reduce_tid /= 2;
			reduce_thread_count /= 2;
			reduce_index += 1;
		}
	}
}


void* thread_function(void *arg_ptr) {
	thread_arg_t *arg = (thread_arg_t*) arg_ptr;
	thread_affinity(arg->tid);

	pthread_mutex_lock(&arg->start_cond.mutex);
	while(true) {
		thread_init_signal(arg);
		pthread_cond_wait(&arg->start_cond.condition, &arg->start_cond.mutex);
		if(arg->status == THREAD_CANCEL) break;
		arg->status = THREAD_RUNNING;

		arg->loop_function(arg_ptr);
		if(arg->reduce) {
			reduce_plus(arg);
		}

		int i = 0;
		arg->end_cond.tid = arg->tid;
		thread_cond_signal(&arg->end_cond, NULL);
	}

	pthread_mutex_unlock(&arg->start_cond.mutex);
	arg->status=THREAD_FINISHED;
	return (void *)NULL;
}

thread_arg_t *get_thread_array(unsigned num) {
	if(num <= threads_size) {
		return thread_arguments;
	}

	// search max threads number
	unsigned_huge max = 1, act = 0;
	range_reset(config.threads);
	while(range_next(config.threads, &act)) {
		if(max < act) max = act;
	}
	if(threads == NULL)
		threads = malloc(max*sizeof(pthread_t));
	if(threads == NULL) {
		_printf("WARNING: couldn't allocate thread array\n");
		return NULL;
	}
	if(thread_arguments == NULL)
	thread_arguments = malloc(sizeof(thread_arg_t)*(max));
	if(thread_arguments == NULL) {
		_printf("WARNING: couldn't allocate thread arguments array\n");
		return NULL;
	}

	int i;
	for(i=0; i<threads_size; i++) {
		thread_arguments[i].allargs = thread_arguments;
	}
	for(i=threads_size; i<num; i++) {
		pthread_attr_t thread_attributes;
		pthread_attr_init(&thread_attributes);
		pthread_attr_setstacksize(&thread_attributes, PTHREAD_STACK_MIN);
		pthread_attr_setdetachstate(&thread_attributes, PTHREAD_CREATE_DETACHED);

		thread_arg_t init = THREAD_ARG_T_INIT;
		thread_arguments[i] = init;
		thread_arguments[i].thread = threads;
		thread_arguments[i].tid = i;
		thread_arguments[i].thread_count = i;
		thread_arguments[i].allargs = thread_arguments;
		thread_arguments[i].status = THREAD_CREATED;

		int err = pthread_create(&threads[i], &thread_attributes, thread_function, (void *)(&thread_arguments[i]));
		if(err != 0) {
			_printf("Error creating thread: %d\n", err);
			return NULL;
		}
	}
	threads_size = num;
	return thread_arguments;
}

void finish_threads() {
	int i;
	for(i=0; i<threads_size; i++) {
		thread_arguments[i].status = THREAD_CANCEL;
		thread_cond_signal(&thread_arguments[i].start_cond, NULL);
	}
	for(i=0; i<threads_size; i++) {
		while(thread_arguments[i].status != THREAD_FINISHED)
			usleep(100);
		thread_cond_destroy(&thread_arguments[i].init_cond);
		thread_cond_destroy(&thread_arguments[i].start_cond);
		thread_cond_destroy(&thread_arguments[i].end_cond);
		thread_cond_destroy(&thread_arguments[i].reduce_cond);
	}
	threads_size = 0;
	free(thread_arguments);
	free(threads);
}

void thread_affinity(int threadid) {
	if(config.thread_affinity == AFFINITY_ROUND_ROBIN) {
		pthread_t ppid = pthread_self();
		cpu_set_t mask;
		CPU_ZERO(&mask);
		int processorid = get_processorid_recommendation(threadid);
		CPU_SET(processorid, &mask);

		int result = pthread_setaffinity_np(ppid, sizeof(mask), &mask);
		if(result != 0){
			_printf("WARNING: failed to set cpu affinity\n");
		}
		else {
			if(config.verbose > 1) {
				_printf("affinity of thread %02d: processorid %02d\n", threadid, processorid);
			}
		}
	}
}
