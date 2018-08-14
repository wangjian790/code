/* This is the MPI program computing the value of PI */
#include "mpi.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
  int done = 0, n, myid, numprocs, i, rc;
  double PI25DT = 3.141592653589793238462643;
  double mypi, pi, h, sum, x, a;
  int block;

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD,&numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD,&myid);
  if (argc != 2) {
    if (myid == 0) {
      printf("Usage: a.out N\n");
    } 
    MPI_Finalize();
    exit(0);
  }
  n = atoi(argv[1]);

  h   = 1.0 / (double) n;
  sum = 0.0;

  block = n / numprocs;

  if (myid != numprocs -1) {
    for (i = myid * block + 1; i<= (myid+1)*block; i ++) {
      x = h * ((double)i - 0.5);
      sum += 4.0 / (1.0 + x*x);
    }
  } else { /* myid == numprocs */
    for (i=myid * block+1; i <=n; i++) {
      x = h * ((double)i - 0.5);
      sum += 4.0 / (1.0 + x*x);
    }
  }


  mypi = h * sum;
  MPI_Reduce(&mypi, &pi, 1, MPI_DOUBLE, MPI_SUM, 0,MPI_COMM_WORLD);
  if (myid == 0) 
    printf("pi is approximately %.16f, Error is %.16f\n", pi, fabs(pi - PI25DT));
  MPI_Finalize();
  return 0;
}
