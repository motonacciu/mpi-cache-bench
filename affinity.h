
#include <cstdio>

#include <sched.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>

#include <stdexcept>

// In order to get stable readings from the PAPI library we force processes to specific 
// cores, therefore the OS will not move them around during the execution of the benchmark
void set_process_affinity(int rank, size_t map[]){
	// set the process affinity
	pid_t pid = getpid();
	cpu_set_t mask;

	CPU_ZERO(&mask);
	CPU_SET(map[rank], &mask);

	if(sched_setaffinity(pid, sizeof(cpu_set_t), &mask) != 0){
		char msg[1000];
		sprintf(msg, "ERROR: could not set pid %d's (rank %i) affinity to %i.\n", pid, rank, mask);

		throw std::logic_error(std::string(msg));
	}
}
