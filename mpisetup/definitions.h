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

#define PRECISSION "18.15"
#define BIG_PRECISSION "20.15"

extern int (*_printf)(const char *, ...);
#endif
