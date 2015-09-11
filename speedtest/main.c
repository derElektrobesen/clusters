#include <stdio.h>
#include <mpi.h>

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int world_size = 0;
    MPI_Comm_size(MPI_COMM_WORLD, &world_size);

    int world_rank = 0;
    MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);

    char processor_name[MPI_MAX_PROCESSOR_NAME] = "";
    int name_len = 0;

    MPI_Get_processor_name(processor_name, &name_len);

    printf("Processor: %.*s, world_rank: %d, world_size: %d\n",
            name_len, processor_name, world_rank, world_size);

    MPI_Finalize();
    return 0;
}
