mpi-cache-bench
===============

A benchmark code to study the impact of CPU caches on MPI communication routines

Compile
=======

Run make to compile the cache_bench executable. 

Set the CXX environment variable to point to mpicxx.

Run
===

The mpi-cache-bench supports only 2 MPI processes. Therefore in order to run the benchmark you need to run the following command:

mpirun -np 2 ./cache_bench 1 8 6M 

Where the parameters are respectively: 
1) number of sockets in the system 
2) total number of cores in the system 
3) amount of last level cache for a single CPU
