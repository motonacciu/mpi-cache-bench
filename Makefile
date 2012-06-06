
CXX=mpicxx
CXXFLAGS = -I. -O3 -DREPETITIONS=10

PAPI_HOME=/usr
MPI_HOME=/usr

CXXFLAGS += -I$(PAPI_HOME)/include
LDFLAGS  += -L$(PAPI_HOME)/lib -lpapi

CXXFLAGS += -I$(MPI_HOME)/include 
LDFLAGS  += -L$(MPI_HOME)/lib -lmpi -lmpi_cxx

#CXXFLAGS += -I$(HWLOC_HOME)/include
#LDFLAGS  += -L$(HWLOC_HOME)/lib -lhwloc

cache_bench: cache_bench.cpp papi_wrap.o 

papi_wrap.o: papi_wrap.h papi_wrap.cpp

clean:
	rm -f cache_bench cache_bench.o papi_wrap.o
