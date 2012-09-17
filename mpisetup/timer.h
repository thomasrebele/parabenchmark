
#ifndef TIMER
#define TIMER

#include "definitions.h"
typedef struct timespec timespec;

extern const byte MODE_START;
extern const byte MODE_END;

double tick(byte modus);


#endif
