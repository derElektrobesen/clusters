#include "speedtest.h"
#include "utils.h"

#ifndef NODES_COUNT
#	error "Nodes count is not defined!"
#endif

static void do_test(int proc_rank) {
	if (proc_rank == MANAGER_RANK) {
		printf("Unexpected process rank: %d", proc_rank);
		return;
	}

	MPI_Group world_group;
	MPI_Comm_group(MPI_COMM_WORLD, &world_group);

	MPI_Group group;
	MPI_Comm comm;
	int comm_group[] = { proc_rank, MANAGER_RANK };

	MPI_Group_incl(world_group, SV_SIZE(comm_group), comm_group, &group);
	MPI_Comm_create(MPI_COMM_WORLD, group, &comm);

	speedtest(comm, MANAGER_RANK);

	MPI_Group_free(&group);
	MPI_Comm_free(&comm);
}

static void do_manage() {
	MPI_Group world_group;
	MPI_Comm_group(MPI_COMM_WORLD, &world_group);

	int comm_group[] = { 0, MANAGER_RANK };
	MPI_Comm comm_list[NODES_COUNT - 1];

	// create a set of cummunicators
	for (int i = MANAGER_RANK + 1; i < NODES_COUNT; ++i) {
		// process only nodes with rank > MANAGER_RANK
		comm_group[0] = i;

		MPI_Group group;
		MPI_Group_incl(world_group, SV_SIZE(comm_group), comm_group, &group);
		MPI_Comm_create(MPI_COMM_WORLD, group, &comm_list[i - 1]);

		MPI_Group_free(&group);
	}

	manage(SV_SIZE(comm_list), comm_list, MANAGER_RANK);

	// destruct comm group
	for (int i = MANAGER_RANK + 1; i < NODES_COUNT; ++i) {
		MPI_Comm_free(&comm_list[i - 1]);
	}
}

int main(int argc, char **argv) {
	MPI_Init(&argc, &argv);

	int world_rank = 0;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

	int world_size = 0;
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);

	set_rank(world_rank);

	if (world_size != NODES_COUNT) {
		warn("Must specify MP_PROCS=%d. Terminating.\n", NODES_COUNT);
		MPI_Finalize();
		return 0;
	}

	if (world_rank == MANAGER_RANK) {
		do_manage();
	} else {
		do_test(world_rank);
	}

	MPI_Finalize();
	return 0;
}
