/*
 * definitions.h
 *
 * This file provides definitions common to all files
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

#ifndef __DEFINITIONS_H
#define __DEFINITIONS_H

#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <time.h>

typedef char byte;
typedef long long huge;
typedef unsigned long long unsigned_huge;

#define TYPE char
#define TYPE_MPI MPI_CHAR

//#define DEBUG
#define GB (1024*1024*1024)
#define MB (1024*1024)
#define KB (1024)

#define PRECISSION "15.11"
#define BIG_PRECISSION "18.11"

extern int (*_printf)(const char *, ...);
#endif
