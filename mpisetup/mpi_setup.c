
#include <mpi.h>
#include <stdio.h>
#include "timer.h"

int main(int argc, char **argv) {
	
	double time;
	tick(MODE_START);
	MPI_Init(&argc, &argv);
	time = tick(MODE_END);
	printf("MPI_Init time: %.10f\n", time);
	
	tick(MODE_START);
	MPI_Finalize();
	time = tick(MODE_END);
	printf("MPI_Finalize time: %.10f\n", time);

}
