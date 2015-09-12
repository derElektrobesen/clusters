#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <stdio.h>
#include <mpi.h>

void speedtest(MPI_Comm comm, int manager_rank);
void manage(int list_size, const MPI_Comm *comm_list, int manager_rank);

#endif // SPEEDTEST_H
