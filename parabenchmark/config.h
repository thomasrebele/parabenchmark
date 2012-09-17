/*
 * config.h
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
#ifndef __CONFIG_H
#define __CONFIG_H

#include "range.h"

typedef struct config_t_ {
	bool output_tee;
	bool output_to_files;
	bool output_system_info;
	bool output_omit_startup_system_info;
	char *output_dir;
	int verbose;

	enum {
		AFFINITY_NONE,
		AFFINITY_ROUND_ROBIN
	} thread_affinity;

	unsigned_huge warmup;

	struct {
		double time_guide_value;
		unsigned_huge number;
		unsigned_huge min;
	} repetitions;

	struct {
		double time_guide_value;
		unsigned_huge number;
		unsigned_huge min;
	} steps;

	// used for memory tests
	struct {
		unsigned_huge cache_clean_size;
		range_t *blocksize;
		range_t *stride;
	} memory;

	// used for mpi
	range_t *processes;
	range_t *threads;
	range_t *range;

} config_t;

config_t default_config;
config_t config;

#endif
