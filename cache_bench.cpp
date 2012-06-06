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

#include "affinity.h"
#include "papi_wrap.h"
#include "hwloc_wrap.h"

#include <mpi.h>

#include <iostream>
#include <iterator>

#include <string>
#include <cassert>

#include <fstream>
#include <cstdlib>

#include <cmath>
#include <cstdlib>
#include <algorithm>

#include <iomanip>
#include <locale>

volatile size_t g_val = 5;
int rank;

unsigned offset = 0;

#define ENABLE_SYNCH

#define CLEAN \
	{ \
	MPI_Barrier(MPI_COMM_WORLD); \
	for(register size_t idx=0, end=std::max(cache_size,size); idx<end; idx+=cache_line) { \
		buff[idx+(idx%(cache_line-1))] += g_val; \
	} \
	}

//Defines the loop utilized to bring the data into the cache. We do this backwards so that 
//we are sure L1 and L2 cache are also filled with the values we are going to access in the
//benchmark
#define _RCOMP \
	{\
	for(register long idx=size-cache_line; idx>=0; idx-=cache_line) { \
		g_val+=msg[idx+(idx%(cache_line-1))]; \
	} \
	}

// This is the computational loop utilized to load the value of the message buffer
// into the cache 
#define RCOMP(x) \
	{\
	reg.start(x);\
	for (register size_t i=0; i<size; i+=cache_line) \
		g_val += msg[i]; \
	reg.end(x);\
	}

// This is the computational loop utilized to load the value of the message buffer
// into the cache 
#define _WCOMP \
	{\
	for(register long idx=size-cache_line; idx>=0; idx-=cache_line) { \
		msg[idx+(idx%(cache_line-1))] += g_val; \
	} \
	} 

// This is the computational loop utilized to load the value of the message buffer
// into the cache 
#define WCOMP(x) \
	{\
	reg.start(x);\
	for (register size_t i=0; i<size; i+=cache_line) \
		msg[i] += g_val; \
	reg.end(x);\
	}

// This is the communication part of the benchmark which send/recv the array between
// the 2 processes 
#define COMM(x) \
	{\
	if (rank == 0) {\
		reg.start(x); \
		PMPI_Send((char*)msg, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);\
		reg.end(x); \
	} else {\
		reg.start(x); \
		PMPI_Recv((char*)msg, size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);\
		reg.end(x); \
	}\
	}

// Warm up the instruction cache 
#define _COMM \
	if (rank == 0) { \
		PMPI_Recv(NULL, 0, MPI_BYTE, 1, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE); \
		PMPI_Send(NULL, 0, MPI_BYTE, 1, 1, MPI_COMM_WORLD); \
	} else { \
		PMPI_Send(NULL, 0, MPI_BYTE, 0, 0, MPI_COMM_WORLD); \
		PMPI_Recv(NULL, 0, MPI_BYTE, 0, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE); \
	} \

#define MEMCPY(x) \
	{ \
	reg.start(x); \
	memcpy((char*)buff, (char*)msg, size); \
	reg.end(x); \
	} 

typedef void (*TestFunc)(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size);

//=============================================================================
// TEST 1: Load array (read mode) from memory when cache is cold
//         [ cached is cleaned up and then msg array is accessed]
//=============================================================================
void test_1(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	RCOMP(101100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 2: Load array (read mode) from memory when cache is hot
//         [ cached is preloaded and then msg array is accessed]
//=============================================================================
void test_2(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	_RCOMP;

	RCOMP(102100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 3: Load array (write mode) from memory when cache is cold and perform a 
// 		   write operation.
// 		   [ cached is cleaned up and then msg array is accessed]
//=============================================================================
void test_3(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	WCOMP(203100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 4: Load array (write mode) from memory when cache is hot and perform a 
//         write operation
//         [ cached is preloaded and then msg array is accessed]
//=============================================================================
void test_4(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	_RCOMP;

	WCOMP(204100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 5: Send/recv array from memory when cache is cold
//=============================================================================
void test_5(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size,  size_t cache_line, size_t size) {

	CLEAN;
	
	_COMM;

	COMM(305200+offset);

#ifdef ENABLE_SYNCH
	_COMM;
#endif
}

//=============================================================================
// TEST 6: Send/recv array from memory when cache is hot (read)
//=============================================================================
void test_6(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// clean up everything
	CLEAN;

	// Load array into cache
	_RCOMP;

	_COMM;

	COMM(306200+offset);

#ifdef ENABLE_SYNCH
	_COMM;
#endif
}

//=============================================================================
// TEST 8: Send/recv array from memory when cache is hot (write)
//=============================================================================
void test_7(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	// Load array into cache
	_WCOMP;

	_COMM;

	COMM(307200+offset);

#ifdef ENABLE_SYNCH
	_COMM;
#endif
}

//=============================================================================
// TEST 8: Send/recv array from memory when cache is hot (write)
//=============================================================================
void test_8(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// Load array into cache
	CLEAN;

	memcpy(NULL, NULL, 0);

	MEMCPY(408100 + offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 8: Send/recv array from memory when cache is hot (write)
//=============================================================================
void test_9(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;
	// Load array into cache
	memcpy(NULL, NULL, 0);

	_RCOMP;

	MEMCPY(409100 + offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 11: Send/recv array from memory when cache is hot (write)
//=============================================================================
void test_10(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;
	// Load array into cache
	
	memcpy(NULL, NULL, 0);

	_WCOMP;

	MEMCPY(410100 + offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

//=============================================================================
// TEST 1: Load array (read mode) from memory when cache is cold
//         [ cached is cleaned up and then msg array is accessed]
//=============================================================================
void test_20(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	RCOMP(720100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_21(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	_RCOMP;

	MPI_Barrier(MPI_COMM_WORLD);

	RCOMP(721100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_22(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// Load array into cache
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		MPI_Send((char*)msg, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Recv((char*)msg, size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	RCOMP(722100+offset);
#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_23(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// Load array into cache
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	_RCOMP

	MPI_Barrier(MPI_COMM_WORLD);


	if (rank == 0) {
		MPI_Send((char*)msg, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Recv((char*)msg, size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	RCOMP(723100+offset);
#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_30(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	WCOMP(830100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_31(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	_WCOMP;

	MPI_Barrier(MPI_COMM_WORLD);

	WCOMP(831100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_32(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// Load array into cache
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		MPI_Send((char*)msg, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Recv((char*)msg, size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	WCOMP(832100+offset);

#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

void test_33(RegionCounter& reg, volatile char* msg, volatile char* buff, size_t cache_size, size_t cache_line, size_t size) {
	// Load array into cache
	CLEAN;

	MPI_Barrier(MPI_COMM_WORLD);

	_WCOMP

	MPI_Barrier(MPI_COMM_WORLD);

	if (rank == 0) {
		MPI_Send((char*)msg, size, MPI_BYTE, 1, 0, MPI_COMM_WORLD);
	} else {
		MPI_Recv((char*)msg, size, MPI_BYTE, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	WCOMP(833100+offset);
#ifdef ENABLE_SYNCH
	MPI_Barrier(MPI_COMM_WORLD);
#endif
}

class BenchBinder {

	TestFunc	func_ptr;
	volatile char*	msg_ptr;
	volatile char*  buff_ptr;
	size_t 		cache_size;
	size_t 		curr_size;
	unsigned 	cache_line;

public:
	BenchBinder(const TestFunc& func_ptr, 
				volatile char* msg_ptr, 
				volatile char* buff_ptr, 
				size_t cache_size, 
				size_t curr_size, 
				unsigned cache_line) 
		: func_ptr(func_ptr),
		  msg_ptr(msg_ptr), 
		  buff_ptr(buff_ptr), 
		  cache_size(cache_size), 
		  curr_size(curr_size), 
		  cache_line(cache_line) { } 

	inline void operator()(RegionCounter& reg) const {
		return func_ptr(reg, msg_ptr, buff_ptr, cache_size, cache_line, curr_size);
	}
};


void measure(unsigned rep, std::ostream& logFile, const EventNames& evts, size_t cache_size, size_t cache_line_size) 
{
	
	TestFunc benchs[] = {
			//  &test_1,  &test_2,
			&test_5,  &test_6, &test_7,
			//&test_8,  &test_9 , &test_10,
			test_20, test_21, test_22, test_23,
			test_30, test_31, test_32, test_33,
		};

	offset = 0; 

	MPI_Barrier(MPI_COMM_WORLD);
	!rank && std::cout << "~~~> Benchmark STARTS <~~~" << std::endl;
	!rank && std::cout << "     + Don't move and hold your breath" << std::endl;

	for (register size_t size = 64; size <= cache_size*4; size*=2) {
		
		MPI_Barrier(MPI_COMM_WORLD);
		!rank && std::cout << "Measuring for size: " << size << std::endl;
		
		++offset;

		size_t buff_size = std::max(cache_size, size);
		volatile char* msg = new char[ 2 * buff_size ];
		// printf("BUFF: %x - %x\n", buff, (buff + buff_size));

		volatile char* buff  = &msg[ buff_size ];
		// printf("MSG: %x - %x\n", msg, (msg + buff_size));
		memset((char*)msg, sizeof(char) * 2 * buff_size, 2);

		for(size_t idx=0; idx<11; ++idx) {
			measure(logFile, evts, BenchBinder(benchs[idx], msg, buff, cache_size, size, cache_line_size), rep);
			!rank && std::cout << "%" << std::flush;
		}
		!rank && std::cout << std::endl;

		delete[] msg;
	}
}

size_t read_counter_names(const std::string& file_name, std::vector<std::string>& counter_names) {
	size_t max_lenght=0;
	try {
		std::ifstream file(file_name.c_str(), std::ifstream::in);

		std::string line;
		while(std::getline(file, line)) { 
			line.erase(std::remove_if(line.begin(), line.end(), isspace), line.end());
			counter_names.push_back(line); 
			max_lenght = std::max(max_lenght, line.length());
		}
	}catch(std::exception e) { 
		std::cerr << "Error while reading file: " << file_name << std::endl;
	}
	return max_lenght;
}

int main (int argc, char* argv[]) {

	MPI_Init(NULL, NULL);

	int comm_size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &comm_size);


	// Read the PAPI_HOME environment variable 
	std::string PAPI_HOME = getenv("PAPI_HOME") ? getenv("PAPI_HOME") : "";
	if (PAPI_HOME.length() != 0) {
		PAPI_HOME = PAPI_HOME+"/bin/";
	}
	std::cout << "PAPI_HOME=" << PAPI_HOME << std::endl;

	std::vector<std::string> evts;
	// Read the available events from PAPI
	char rankStr[30];
	sprintf(rankStr, "%d", rank);
	std::string fileName = std::string("/tmp/hw_counters_") + rankStr + ".txt";
	system((PAPI_HOME + "papi_avail -a | grep ^PAPI_| cut -d \" \" -f 1 > " + fileName).c_str());

	MPI_Barrier(MPI_COMM_WORLD);

	// get the hostname of the two processes
	char hostname[30];
	gethostname(hostname, 30);
	std::cout << "[R" << rank << "]: Processes allocated on node " << hostname << std::endl;

	char (*hosts)[30];
	if (rank == 0) {
		hosts = new char[comm_size][30];
	}

	MPI_Gather(hostname, 30, MPI_CHAR, hosts, 30, MPI_CHAR, 0, MPI_COMM_WORLD);
	int sameHost=0;
	if (rank==0 && (std::string(hosts[0]) == std::string(hosts[1]))) {
		sameHost=1;
	}
	MPI_Bcast(reinterpret_cast<void*>(&sameHost), 1, MPI_INT, 0, MPI_COMM_WORLD);

	std::cout << "[R" << rank << "]: Same host " << sameHost << std::endl;

	if (rank == 0) { delete[] hosts; }

	size_t length = 0;
	length = std::max(length, read_counter_names(fileName, evts));
	length = std::max(length, read_counter_names("./counters.txt", evts));
	length+=2;

	std::cout << "Number of PAPI counters: " << evts.size() << std::endl;

	// open log files
	std::string logFileName = std::string("cache_bench.r") + rankStr + ".csv";
	std::fstream logFile(logFileName.c_str(), std::fstream::out | std::fstream::trunc);

	Info info(argc, argv);

	if (rank == 0) {
		std::cout << "@@ Num cores: " << info.num_cores << std::endl;
		std::cout << "@@ Num sockets: " << info.num_sockets << std::endl;
	}

	size_t cache_size = info.cache_sizes[info.levels-1];
	std::cout << "@@ Total last level cache size per CPU is: " << cache_size << std::endl;

	if (sameHost!=0 && info.num_sockets==1) {
		// shared cache
		std::cout << "Running on shared cache" << std::endl;
	}

	unsigned affinity = 0;
	// find best affinity for the benchmark
	if (sameHost==1 && info.num_sockets==1) {
		affinity = info.num_cores-1;
		std::cout << "MPI Processes running on same CPU" << std::endl;
	}
	if (sameHost==1 && info.num_sockets!=1) {
		affinity = info.num_cores/info.num_sockets;
	}

	std::cout << "[R" << rank <<"] Affinity set to: {0, " << affinity << "}" << std::endl;

	set_process_affinity(rank, (size_t[2]){ 0, affinity });

	logFile << std::setw(8) << "id" << std::setw(10) << "time";

	for(EventNames::const_iterator it=evts.begin(), end=evts.end(); it!=end; ++it) { logFile << std::setw(length) << *it; }
	logFile << std::flush << std::endl;
	
	std::cout << "Cache size is: " << cache_size << std::endl;

	std::cout << "**** Warmup channels ****" << std::endl;
	for(unsigned i=0; i<100; ++i) { 
		int data=0;
		if(rank==0) { 
			MPI_Send(&data,1,MPI_INT,1,0,MPI_COMM_WORLD);
		} else { 
			MPI_Recv(&data,1,MPI_INT,0,0,MPI_COMM_WORLD, MPI_STATUS_IGNORE); 
		}
	}

	measure(REPETITIONS, logFile, evts, cache_size, 64);

	logFile.close();
	std::cout << g_val << std::endl;

	MPI_Finalize();

}

