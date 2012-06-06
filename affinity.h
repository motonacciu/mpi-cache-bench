/**
 *  This file is part of mpi-cache-bench.
 *
 *  mpi-cache-bench is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mpi-cache-bench is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mpi-cache-bench.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

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
