/*
 * timer.h
 *
 * Helper functions for measuring time
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

#ifndef __TIMER_H
#define __TIMER_H

#include "definitions.h"
typedef struct timespec timespec_t;

extern const byte MODE_START;
extern const byte MODE_END;

void calibrate_timer();
void calibrate_utime_timer();
void calibrate_context_switch_stat();
unsigned context_switch_count();

double tick(byte modus);
double tick2(byte modus, double *tmp);
#ifdef COMPILE_WITH_MPI
#include <mpi.h>
double tick_mpi(byte modus, MPI_Comm barrier);
#endif

#endif
