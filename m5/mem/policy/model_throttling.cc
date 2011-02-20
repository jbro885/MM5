/*
 * model_throttling.cc
 *
 *  Created on: Jan 9, 2011
 *      Author: jahre
 */

#include "model_throttling.hh"

ModelThrottlingPolicy::ModelThrottlingPolicy(std::string _name,
			   	    			 InterferenceManager* _intManager,
			   	    			 Tick _period,
			   	    			 int _cpuCount,
			   	    			 PerformanceEstimationMethod _perfEstMethod,
			   	    			 bool _persistentAllocations,
			   	    			 int _iterationLatency,
			   	    			 Metric* _performanceMetric,
			   	    			 bool _enforcePolicy)
: BasePolicy(_name, _intManager, _period, _cpuCount, _perfEstMethod, _persistentAllocations, _iterationLatency, _performanceMetric, _enforcePolicy)
{
	//enableOccupancyTrace = true;

	mshrOccupancyPtrs.resize(_cpuCount, NULL);
}

void
ModelThrottlingPolicy::runPolicy(PerformanceMeasurement measurements){

	DPRINTF(MissBWPolicy, "--- Running Model Throttling Policy\n");

	assert(currentMeasurements == NULL);
	currentMeasurements = &measurements;

	for(int i=0;i<mshrOccupancyPtrs.size();i++){
		assert(mshrOccupancyPtrs[i] == NULL);
		mshrOccupancyPtrs[i] = intManager->getMSHROccupancyList(i);
	}

	updateMWSEstimates();
	updateAloneIPCEstimate();
	measurements.computedOverlap = computedOverlap;
	traceVector("Alone IPC Estimates: ", aloneIPCEstimates);
	traceVector("Estimated Alone Cycles: ", aloneCycles);
	traceVector("Computed Overlap: ", computedOverlap);

	vector<double> optimalArrivalRates = findOptimalArrivalRates(&measurements);
	setArrivalRates(optimalArrivalRates);

	// clean up
	for(int i=0;i<mshrOccupancyPtrs.size();i++) mshrOccupancyPtrs[i] = NULL;
	intManager->clearMSHROccupancyLists();

	currentMeasurements = NULL;
}

bool
ModelThrottlingPolicy::doEvaluation(int cpuID){
	fatal("model throttling doEvaluation not implemented");
	return false;
}

std::vector<double>
ModelThrottlingPolicy::findOptimalArrivalRates(PerformanceMeasurement* measurements){
	vector<double> optimalPeriods = performanceMetric->computeOptimalPeriod(measurements, aloneCycles, cpuCount);

	vector<double> optimalRequestRates = vector<double>(cpuCount, 0.0);

	assert(optimalPeriods.size() == optimalRequestRates.size());
	for(int i=0;i<optimalPeriods.size();i++){
		optimalRequestRates[i] = ((double) measurements->requestsInSample[i]) / optimalPeriods[i];
	}

	traceVector("Got optimal periods: ", optimalPeriods);
	traceVector("Request count: ", measurements->requestsInSample);
	traceVector("Returning optimal request rates: ", optimalRequestRates);

	return optimalRequestRates;
}

void
ModelThrottlingPolicy::setArrivalRates(std::vector<double> rates){

	vector<double> cyclesPerReq = vector<double>(rates.size(), 0.0);
	for(int i=0;i<rates.size();i++){
		cyclesPerReq[i] = 1.0 / rates[i];
	}

	traceVector("Optimal cycles per request: ", cyclesPerReq);

	for(int i=0;i<caches.size();i++){
		DPRINTF(MissBWPolicy, "Setting minimum request interval for CPU %d to %d\n", i, (int) cyclesPerReq[i]);
		caches[i]->setMinRequestInterval(cyclesPerReq[i]);
	}
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	SimObjectParam<InterferenceManager* > interferenceManager;
	Param<Tick> period;
	Param<int> cpuCount;
	Param<string> performanceEstimationMethod;
	Param<bool> persistentAllocations;
	Param<int> iterationLatency;
	Param<string> optimizationMetric;
	Param<bool> enforcePolicy;
END_DECLARE_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

BEGIN_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)
	INIT_PARAM_DFLT(interferenceManager, "The system's interference manager" , NULL),
	INIT_PARAM_DFLT(period, "The number of clock cycles between each decision", 1048576),
	INIT_PARAM(cpuCount, "The number of cpus in the system"),
	INIT_PARAM(performanceEstimationMethod, "The method to use for performance estimations"),
	INIT_PARAM_DFLT(persistentAllocations, "The method to use for performance estimations", true),
	INIT_PARAM_DFLT(iterationLatency, "The number of cycles it takes to evaluate one MHA", 0),
	INIT_PARAM_DFLT(optimizationMetric, "The metric to optimize for", "hmos"),
	INIT_PARAM_DFLT(enforcePolicy, "Should the policy be enforced?", true)
END_INIT_SIM_OBJECT_PARAMS(ModelThrottlingPolicy)

CREATE_SIM_OBJECT(ModelThrottlingPolicy)
{

	BasePolicy::PerformanceEstimationMethod perfEstMethod =
		BasePolicy::parsePerformanceMethod(performanceEstimationMethod);

	Metric* performanceMetric = BasePolicy::parseOptimizationMetric(optimizationMetric);

	return new ModelThrottlingPolicy(getInstanceName(),
							         interferenceManager,
							         period,
							         cpuCount,
							         perfEstMethod,
							         persistentAllocations,
							         iterationLatency,
							         performanceMetric,
							         enforcePolicy);
}

REGISTER_SIM_OBJECT("ModelThrottlingPolicy", ModelThrottlingPolicy)

#endif //DOXYGEN_SHOULD_SKIP_THIS


