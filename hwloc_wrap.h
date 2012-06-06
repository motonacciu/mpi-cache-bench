
/**
 * When the benchmark is compiled with HWLOC support then we use the library to detect 
 * information of the underlying CPU, like the amount of cache for each level of 
 * available caches and the number of sockets and cores available on the machine 
 */
#ifdef USE_HWLOC
#include <hwloc.h>
#endif

#include <cstring>

void usage(char* argv[]) { 
	std::cerr << "Argument error, usage: " << argv[0] << 
				 " SOCKETS_PER_NODE CPU_CORES_PER_NODE LAST_LEVEL_CACHE" << std::endl
		 	  << "\tLAST_LEVEL_CACHE format: XX, XXK, XXM, XXG (where X is a digit)"
			  << std::endl;
	exit(1);
}

#define MAX_CACHE_LEVELS 10
// Stores the number of cores / sockets and the cache size exclusive to each process
struct Info {
	unsigned num_cores;
	unsigned num_sockets;
	unsigned levels;
	// For each level contains the size of the cache at that level 
	size_t* cache_sizes;

	Info(int argc, char* argv[]) : levels(0) {

#ifndef USE_HWLOC
		if (argc != 4) { usage(argv); }

		num_sockets = atoi(argv[1]);
		num_cores = atoi(argv[2]);

		levels=1;
		cache_sizes = new size_t[1];

		char* size_str = argv[3];
		unsigned multiplier = 1;

		char* last = &size_str[strlen(size_str)-1];
		if (!(*last >=0 && *last <= 9)) { 
			switch (*last) {
			case 'G': multiplier *= 1024;
			case 'M': multiplier *= 1024;
			case 'K': multiplier *= 1024;
					  break;
			default:
				std::cerr << "Wrong quantifer, allowed quantifiers are: 'K' (1024), 'M' (1024K), 'G' (1024M)";
				usage(argv);	
			}
		}
		*last = '\0';
		cache_sizes[0] = atoi(size_str) * multiplier;
#endif

#ifdef USE_HWLOC
		hwloc_topology_t topology;

		// Allocate and initialize topology object.
		hwloc_topology_init(&topology);
		hwloc_topology_load(topology);

		int depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_SOCKET);
		num_sockets = hwloc_get_nbobjs_by_depth(topology, depth);

		depth = hwloc_get_type_or_below_depth(topology, HWLOC_OBJ_PU);  // logical CPU
		num_cores = hwloc_get_nbobjs_by_depth(topology, depth);

		size_t size[MAX_CACHE_LEVELS];
		for (hwloc_obj_t obj = hwloc_get_obj_by_type(topology, HWLOC_OBJ_PU, 0);
			 obj;
			 obj = obj->parent)
		if (obj->type == HWLOC_OBJ_CACHE) {
			assert( levels < MAX_CACHE_LEVELS && "This architecture has more than 3 cache levels");
			size[levels++] = obj->attr->cache.size;
		}

		hwloc_topology_destroy(topology);

		cache_sizes = new size_t[levels];
		memcpy(cache_sizes, size, levels*sizeof(size_t));
#endif 
	}

	~Info() {
		delete[] cache_sizes;
	}

private:
	Info(const Info& other) { } // make it not copyable
};

