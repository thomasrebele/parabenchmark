
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	
	MPI_Init(&argc, &argv);

	char *buffer = (char*)calloc(1024,sizeof(char));
	FILE *output = popen("hostname", "r");
	if(output) {
		if(fgets(buffer, 1024-1, output) != NULL) {
			printf("%s", buffer);
		}
		else {
			printf("fgets failed\n");
		}
	}
	else {
		printf("hostname failed\n");
	}

	MPI_Finalize();
	return 0;
}
