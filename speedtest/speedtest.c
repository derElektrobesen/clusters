#include <time.h>
#include <stdlib.h>

#include "speedtest.h"
#include "utils.h"

#define N_SENDINGS	100
#define RAND_BLOCK_LEN	sizeof(int)
#define DATA_SIZE	RAND_BLOCK_LEN * 100000

void manage(int list_size, const MPI_Comm *comm_list, int manager_rank) {
	static char data[DATA_SIZE];

	/*
	srand(time(NULL));
	for (int i = 0; i < sizeof(data) / RAND_BLOCK_LEN; ++i) {
		*(int *)(data + i * RAND_BLOCK_LEN) = rand();
	}
	*/

	for (int i = 0; i < list_size; ++i) {
		// TODO: send different data
		snprintf(data, sizeof(data), "Message from %d to %d\n", manager_rank, i);
		MPI_Bcast(data, sizeof(data), MPI_CHAR, manager_rank, comm_list[i]);
		warn("Message sent to %d", i);
	}
}

void speedtest(MPI_Comm comm, int manager_rank) {
	static char data[DATA_SIZE] = "";
	MPI_Bcast(data, sizeof(data), MPI_CHAR, manager_rank, comm);

	warn("Message received: \"%s\"\n", data);
}
