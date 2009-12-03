/*
 * performance_measurement.hh
 *
 *  Created on: Nov 16, 2009
 *      Author: jahre
 */

#ifndef PERFORMANCE_MEASUREMENT_HH_
#define PERFORMANCE_MEASUREMENT_HH_

#include "mem/requesttrace.hh"

#include <vector>

class PerformanceMeasurement{
private:
	int cpuCount;
	int numIntTypes;
	int maxMSHRs;
	int period;

public:
	std::vector<int> committedInstructions;
	std::vector<int> requestsInSample;

	std::vector<int> cpuStallCycles;

	std::vector<std::vector<double> > mlpEstimate;
	std::vector<std::vector<double> > avgMissesWhileStalled;

	std::vector<double> sharedLatencies;
	std::vector<double> estimatedPrivateLatencies;
	std::vector<std::vector<double> > latencyBreakdown;
	std::vector<std::vector<double> > privateLatencyBreakdown;

	double actualBusUtilization;
	double sharedCacheMissRate;

	PerformanceMeasurement(int _cpuCount, int _numIntTypes, int _maxMSHRs, int _period);

	void addInterferenceData(std::vector<double> sharedLatencyAccumulator,
						     std::vector<double> interferenceEstimateAccumulator,
							 std::vector<std::vector<double> > sharedLatencyBreakdownAccumulator,
							 std::vector<std::vector<double> > interferenceBreakdownAccumulator,
							 std::vector<int> localRequests);

	std::vector<std::string> getTraceHeader();
	std::vector<RequestTraceEntry> createTraceLine();

	double getNonStallCycles(int cpuID, int period);
};


#endif /* PERFORMANCE_MEASUREMENT_HH_ */
