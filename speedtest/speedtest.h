#ifndef SPEEDTEST_H
#define SPEEDTEST_H

#include <stdio.h>
#include <mpi.h>

void speedtest(MPI_Comm comm);
void manage(int list_size, const MPI_Comm *comm_list);

#endif // SPEEDTEST_H
