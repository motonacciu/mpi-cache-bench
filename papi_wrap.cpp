
#include "papi_wrap.h"
#include <iterator>

//#define DEBUG

PapiWrap::PapiWrap() : isCounting(false), evtSet(PAPI_NULL), evtNum(0), tmpValues(NULL) 
{
	int retval = PAPI_is_initialized();
	if (retval) { return; }
		
	retval = PAPI_library_init(PAPI_VER_CURRENT);
	if (retval != PAPI_VER_CURRENT && retval > 0) 
		throw std::logic_error("PAPI library version mismatch!");

	if (PAPI_set_debug(PAPI_VERB_ECONT) != PAPI_OK)
		throw std::logic_error("Cannot set debug mode");

	tmpValues = new long long[num_counters()];
}

size_t PapiWrap::num_counters() const {
	size_t num_hw_counters = PAPI_num_counters();
	if(num_hw_counters <= PAPI_OK)
		throw std::logic_error("PAPI: error getting available hardware counter nunmber!");
	return num_hw_counters;
}

void PapiWrap::set_events(const EventNames& evt_names) {
	size_t size = evt_names.size();

#ifdef DEBUG
	if (evt_names.empty()) {
		std::cout << "[DEBUG] Measuring execution time " << std::endl;
	} else {
		std::cout << "[DEBUG] Setting event ";
		std::copy(evt_names.begin(), evt_names.end(), std::ostream_iterator<std::string>( std::cout, "," ) );
		std::cout << std::endl;
	}
#endif
	
	if ( size > num_counters() ) { throw std::logic_error("Too many event"); }

	if (isCounting) { throw std::logic_error("Impossible to change event set during counting"); }
	
	if (evtSet != PAPI_NULL) {
		PAPI_cleanup_eventset(evtSet);
		PAPI_destroy_eventset(&evtSet);
		evtSet = PAPI_NULL;
	}

	int error_code;
	if((error_code = PAPI_create_eventset(&evtSet)) != PAPI_OK)
		std::logic_error(
			std::string("PAPI error creating EventSet: ") + PAPI_strerror(error_code)
		);

	if (size > 0) {
		EventCodes evt_codes(size);
		for(size_t i=0; i<size; ++i)  {
			PAPI_event_name_to_code(const_cast<char*>(evt_names[i].c_str()), &evt_codes[i]);
		}

		if((error_code = PAPI_add_events(evtSet, &evt_codes.front(), size)) != PAPI_OK)
			throw std::logic_error(
				std::string("PAPI: Error while registering events: ") + PAPI_strerror(error_code)
			);
	}
	evtNum = size;
}

PapiWrap::~PapiWrap() { 
	delete[] tmpValues;

	if (evtSet != PAPI_NULL) {
		PAPI_destroy_eventset(&evtSet);
		evtSet = PAPI_NULL;
	}
	PAPI_shutdown();
}

