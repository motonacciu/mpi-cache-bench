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

#include <papi.h>

#include <stdexcept>
#include <cassert>

#include <vector>
#include <map>
#include <algorithm>
#include <numeric>

#include <iostream>
#include <iomanip>
#include <cstring>

// TYPEDES /////////////////////////////////////////////////////////////////////////////////////////
typedef std::vector<int> 			EventCodes;
typedef std::vector<std::string>	EventNames;

typedef long long					CounterValue;
typedef std::vector<CounterValue> 	CounterValues;

typedef std::pair<CounterValue, CounterValues> TimeValuePair;

class PapiWrap {

	bool 			isCounting;
	CounterValue 	timer_start;

	int  			evtSet;
	size_t 			evtNum;

	CounterValue* 	tmpValues;

public:

	/** 
	 * Initialize PAPI
	 */
	PapiWrap();

	/**
	 * Retrieves the number of HW counters which can be read at once on the underlying CPU
	 */
	size_t num_counters() const;

	/**
	 * Sets the events which will be read when the next start method is invoked
	 */
	void set_events(const EventNames& evt_names);

	inline void start() {
		assert(!isCounting && evtSet != PAPI_NULL && "Preconditions not satisfied!");

		// reset the values in tmpValues 
		memset(tmpValues, 0, sizeof(CounterValue)*num_counters());

		isCounting = true;
		if (evtNum != 0) {
			int error_code;
			if((error_code = PAPI_start(evtSet)) != PAPI_OK) {
				isCounting = false;
				throw std::logic_error(
					std::string("PAPI: Error while starting counters: ") + PAPI_strerror(error_code)
				);
			}
		}
		timer_start = PAPI_get_real_cyc();
	}

	inline TimeValuePair read() {

		long long timer_end = PAPI_get_real_cyc();
		assert(isCounting && "start() must be invoked first");

		if (evtNum == 0) {
			isCounting = false;
			return std::make_pair(timer_end-timer_start, CounterValues());
		}

		int error_code;
		if ((error_code = PAPI_stop(evtSet, tmpValues)) != PAPI_OK) {
			throw std::logic_error(
				std::string("PAPI: Error while reading counters: ") + PAPI_strerror(error_code)
			);
		}
		
		isCounting = false;	
		return std::make_pair(timer_end-timer_start, CounterValues(tmpValues, tmpValues+evtNum));
	}

	~PapiWrap();
};

/**
 * Computes the average value given an array of elements
 */
template <class IterT>
double avg(const IterT& begin, const IterT& end) {
	return static_cast<double>(std::accumulate(begin, end, 0.0)) / std::distance(begin,end);
}

/** 
 * Compute the standard deviation given an array of elements 
 */
template <class IterT>
double stdev(const IterT& begin, const IterT& end) {
	double avg_val = avg(begin, end);

	struct error {
		typedef typename IterT::value_type elem_t;

		error(const elem_t& avg) : mAvg(avg) { }

		elem_t operator()(const elem_t& v1, const elem_t& v2) const {
			return v1 + pow(v2-mAvg,2);
		}
	private:
		elem_t mAvg;
	};

	return sqrt(
		static_cast<double>(std::accumulate(begin,end, 0.0, error(avg_val))) / (std::distance(begin,end)-1)
	);
}


/**
 * Run a code region with multiple sections reading the values of the counters associated to each
 * ID.
 */
struct RegionCounter {

	typedef unsigned long RegionID;

	struct RegionCounters {
		RegionID 		id;
		CounterValue 	time;
		CounterValues 	values;

		RegionCounters(const RegionID& id, CounterValue time, const CounterValues& values) :
			id(id), time(time), values(values) { 	}
	};

	typedef std::map<RegionID, TimeValuePair> RegionMap;


	RegionCounter(const EventNames& counter_names) :
		counter_names (counter_names), curr_counter(-1), available(false) { }

	bool isDone() const { return curr_counter == counter_names.size(); }
	bool next() { return ++curr_counter == counter_names.size(); }

	inline void start(const RegionID& id) {
		try {
			EventNames counters;
			if (curr_counter != -1) {
				counters.push_back(counter_names[curr_counter]);
			} 
			wrapper.set_events( counters );
			
			wrapper.start();
			available = true;
		} catch(const std::logic_error& e) { // std::cerr << "EXCEPTION: " << e.what() << std::endl; 
		}
	}

	inline void end(const RegionID& id) {
		TimeValuePair ret = available ? 
			wrapper.read() : 
			TimeValuePair(0, CounterValues(1));

		RegionMap::iterator fit = counter_values.find(id);
		if (fit == counter_values.end()) {
			assert(curr_counter == -1);
			fit = counter_values.insert(
					std::make_pair(id, std::make_pair(ret.first, CounterValues()))
				).first;
		}
		RegionMap::value_type& entry = *fit;
		std::copy(ret.second.begin(), ret.second.end(), std::back_inserter(entry.second.second));
		available = false;
	}

	inline std::vector<RegionCounters> values() const { 
		std::vector<RegionCounters> ret;
		for(RegionMap::const_iterator it=counter_values.begin(), end=counter_values.end(); it != end; ++it) 
			ret.push_back( RegionCounters(it->first, it->second.first, it->second.second) );
		return ret;
	}

private:
	PapiWrap 		wrapper;
	EventNames 		counter_names;

	int 			curr_counter;
	bool			available;

	RegionMap 		counter_values;
};


// Measuring Function ///////////////////////////////////////////////////////////////////////////////////

template <class FuncTy>
inline void measure(std::ostream& log, const EventNames& evts, const FuncTy& func, size_t rep = 10) {

	for (unsigned idx=0; idx<rep; ++idx) {

		RegionCounter reg(evts);
		// measure the time only
		func(reg);
		////////////////////////
		
		while(!reg.next()) {
			//////////////
			func(reg);
			//////////////
		}

		const std::vector<RegionCounter::RegionCounters>& values = reg.values();

		for (std::vector<RegionCounter::RegionCounters>::const_iterator it=values.begin(), end=values.end(); it!=end; ++it) {
			log << std::setw(10) << it->id 
				<< std::setw(15) << it->time;
			// Write the valueas of the counters 
			for (CounterValues::const_iterator vit=it->values.begin(), vend=it->values.end(); vit!=vend; ++vit) {
				log << std::setw(25) << *vit;
			}
			log << std::flush << std::endl;
		}
	}	

}


