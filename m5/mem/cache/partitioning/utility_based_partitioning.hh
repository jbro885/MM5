/*
 * utility_based_partitioning.hh
 *
 *  Created on: May 2, 2010
 *      Author: jahre
 */

#ifndef UTILITY_BASED_PARTITIONING_HH_
#define UTILITY_BASED_PARTITIONING_HH_

#include "mem/cache/partitioning/cache_partitioning.hh"

class UtilityBasedPartitioning : public CachePartitioning{

private:
	bool first;

	vector<vector<int> > currentHitDistributions;

	int bestHits;
	vector<int> bestAllocation;

	typedef enum{
		UCP_SEARCH_EXHAUSTIVE,
		UCP_SEARCH_LOOKAHEAD
	} UCPSearchAlgorithm;

	UCPSearchAlgorithm searchAlgorithm;

	RequestTrace allocationTrace;
	std::vector<RequestTrace> hitCurveTraces;

	void enumerateAllocations(vector<int> currentAllocation);

	void evaluateAllocation(vector<int> allocation);

	void traceMissCurves();

public:
    UtilityBasedPartitioning(std::string _name,
						     int _associativity,
						     Tick _epochSize,
						     int _np,
						     CacheInterference* ci,
							 std::string _searchAlg);

    void handleRepartitioningEvent();

};

#endif /* UTILITY_BASED_PARTITIONING_HH_ */
