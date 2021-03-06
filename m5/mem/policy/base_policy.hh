/*
 * miss_bandwidth_policy.hh
 *
 *  Created on: Oct 28, 2009
 *      Author: jahre
 */

#include "sim/sim_object.hh"
#include "sim/eventq.hh"
#include "sim/sim_exit.hh"
#include "base/callback.hh"

#include "mem/accounting/interference_manager.hh"
#include "mem/requesttrace.hh"
#include "mem/accounting/memory_overlap_estimator.hh"
#include "mem/accounting/performance_model_measurements.hh"
#include "mem/cache/base_cache.hh"
#include "mem/cache/miss/throttle_control.hh"
#include "mem/bus/bus.hh"

#include "mem/policy/performance_measurement.hh"
#include "mem/policy/metrics/metric.hh"
#include "mem/policy/metrics/hmos_policy.hh"
#include "mem/policy/metrics/fairness_policy.hh"
#include "mem/policy/metrics/stp_policy.hh"
#include "mem/policy/metrics/aggregate_ipc_policy.hh"

#ifndef BASE_POLICY_HH_
#define BASE_POLICY_HH_

class MissBandwidthPolicyEvent;
class MissBandwidthTraceEvent;
class InterferenceManager;
class CacheAccessMeasurement;
class BaseCache;
class Bus;
class OverlapStatistics;
class CriticalPathTableMeasurements;
class PerformanceModelMeasurements;

class BasePolicy : public SimObject{

public:
    typedef enum{
        MWS,
        MLP
    } RequestEstimationMethod;

    typedef enum{
        LATENCY_MLP,
        LATENCY_MLP_SREQ,
        RATIO_MWS,
        NO_MLP,
        NO_MLP_CACHE,
        CPL,          // number 5 (useful for reading the policy trace)
        CPL_CWP,      // number 6
        CPL_DAMP,     // number 7
        CPL_CWP_DAMP, // number 8
        CPL_HYBRID,
        CPL_HYBRID_DAMP,
        CPL_CWP_SER,
        BOIS,
        ITCA,
        PRIVATE_LATENCY_ONLY,
        SHARED_STALL,
        ZERO_STALL,
		CONST_1,
		ASR
    } PerformanceEstimationMethod;

    typedef enum{
    	EXHAUSTIVE_SEARCH,
    	BUS_SORTED,
    	BUS_SORTED_LOG
    } SearchAlgorithm;

    typedef enum{
    	WS_NONE,
    	WS_SHARED,
    	WS_LATENCY,
    	WS_RATIO
    } WriteStallTechnique;

    typedef enum{
    	PBS_NONE,
    	PBS_RATIO,
    	PBS_SHARED
    } PrivBlockedStallTechnique;

    typedef enum{
    	RST_NONE,
    	RST_SHARED,
    	RST_RATIO
    } EmptyROBStallTechnique;

protected:

    bool enableOccupancyTrace;

	Metric* performanceMetric;

	PerformanceEstimationMethod perfEstMethod;
	WriteStallTechnique writeStallTech;
	PrivBlockedStallTechnique privBlockedStallTech;
	EmptyROBStallTechnique emptyROBStallTech;

	InterferenceManager* intManager;
	Tick period;
	MissBandwidthPolicyEvent* policyEvent;
	MissBandwidthTraceEvent* traceEvent;

	std::vector<Addr> cummulativeMemoryRequests;
	std::vector<Addr> cummulativeCommittedInsts;

	SearchAlgorithm searchAlgorithm;
	int iterationLatency;

	RequestTrace aloneIPCTrace;

	RequestTrace measurementTrace;
	std::vector<BaseCache* > caches;
	std::vector<BaseCache* > sharedCaches;
	std::vector<Bus* > buses;
	std::vector<FullCPU*> cpus;

	RequestTrace searchTrace;

	std::vector<double> aloneIPCEstimates;
	std::vector<double> aloneCycles;
	std::vector<double> avgLatencyAloneIPCModel;

	std::vector<double> sharedCPLMeasurements;
	std::vector<double> sharedNonSharedLoadCycles;
	std::vector<double> sharedLoadStallCycles;

	std::vector<double> privateMemsysAvgLatency;
	std::vector<double> sharedMemsysAvgLatency;

	std::vector<double> computedOverlap;
	std::vector<double> lastModelError;
	std::vector<double> lastModelErrorWithCutoff;
	std::vector<int> lastCPLPolicyDesicion;

	int hybridErrorBufferSize;
	std::vector<std::vector<double> > hybridErrorBuffer;
	double hybridDesicionError;

//	bool dumpInitalized;
//	Tick dumpSearchSpaceAt;

	RequestTrace predictionTrace;

	std::vector<double> currentRequestProjection;
	std::vector<double> currentLatencyProjection;
	std::vector<double> currentIPCProjection;
	std::vector<double> currentInterferenceMissProjection;

	std::vector<double> bestRequestProjection;
	std::vector<double> bestLatencyProjection;
	std::vector<double> bestIPCProjection;
	std::vector<double> bestInterferenceMissProjection;

	std::vector<double> requestAccumulator;
	std::vector<double> requestSqAccumulator;
	std::vector<double> avgReqsPerSample;
	std::vector<double> reqsPerSampleStdDev;

	std::vector<RequestTrace> comInstModelTraces;
	std::vector<RequestTrace> perfModelTraces;
	std::vector<Tick> comInstModelTraceCummulativeInst;

	bool usePersistentAllocations;

	RequestTrace numMSHRsTrace;

	std::vector<std::vector<double> > mostRecentMWSEstimate;
	std::vector<std::vector<double> > mostRecentMLPEstimate;

	std::vector<double> asmPrivateModeSlowdownEsts;

	PerformanceMeasurement* currentMeasurements;

	int maxMSHRs;
	int cpuCount;

	bool measurementsValid;

	double maximumDamping;

	void initProjectionTrace(int cpuCount);
	void traceBestProjection();
	void traceNumMSHRs();
	void initNumMSHRsTrace(int cpuCount);
	void initSearchTrace(int cpuCount, SearchAlgorithm searchAlg);

	void initAloneIPCTrace(int cpuCount, bool policyEnforced);

	void traceAloneIPC(std::vector<int> memoryRequests,
			           std::vector<double> ipcs,
			           std::vector<int> committedInstructions,
			           std::vector<int> stallCycles,
			           std::vector<double> avgLatencies,
		               std::vector<std::vector<double> > missesWhileStalled);

	void initComInstModelTrace(int cpuCount);
	void initPerfModelTrace(int cpuCount);

	Stats::Vector<> aloneEstimationFailed;

	Stats::VectorStandardDeviation<> requestAbsError;
	Stats::VectorStandardDeviation<> sharedLatencyAbsError;
	Stats::VectorStandardDeviation<> requestRelError;
	Stats::VectorStandardDeviation<> sharedLatencyRelError;

	void regStats();

	double computeError(double estimate, double actual);

	// Debug trace methods
	void traceVerboseVector(const char* message, std::vector<int>& data);
	void traceVerboseVector(const char* message, std::vector<double>& data);
	void traceVector(const char* message, std::vector<int>& data);
	void traceVector(const char* message, std::vector<double>& data);
	void tracePerformance(std::vector<double>& sharedCycles, std::vector<double>& speedups);

	double computeCurrentMetricValue();

	void updateAloneIPCEstimate();

	void updateMWSEstimates();

	double computeSpeedup(double sharedIPCEstimate, int cpuID);

	double estimateStallCycles(double currentStallTime,
							   double privateStallTime,
							   double currentAvgSharedLat,
							   double newAvgSharedLat,
							   double sharedRequests,
							   int cpuID,
							   int cpl,
							   double privateMissRate,
							   double cwp,
							   Tick boisAloneStallEst,
							   CriticalPathTableMeasurements cptMeasurements);

//	void dumpSearchSpace(std::vector<int>* mhaConfig, double metricValue);

	double squareRoot(double num);

	void updateBestProjections();

	double estimateWriteStallCycles(double writeStall, double avgPrivmodeLat, int numWriteStalls, double avgSharedmodeLat);
	double estimatePrivateBlockedStall(double privBlocked, double avgPrivmodeLat, double avgSharedmodeLat);
	double estimatePrivateROBStall(double writeStall, double avgPrivmodeLat, double avgSharedmodeLat);

	double computeRawError(double estimate, double actual);

	double computeDampedEstimate(double modelEstimate, double sharedModelEstimate, double curStallTime, int cpuID);

	void addHybridError(int cpuID, double error);

	double getHybridAverageError(int cpuID);

	void disableCommitSampling();

	template<typename T>
	std::vector<RequestTraceEntry> getTraceCurve(std::vector<T> data);

	virtual std::vector<RequestTraceEntry> getTraceCurveInt(std::vector<int> data);
	virtual std::vector<RequestTraceEntry> getTraceCurveDbl(std::vector<double> data);

public:

	BasePolicy(std::string _name,
			   InterferenceManager* _intManager,
			   Tick _period,
			   int _cpuCount,
			   PerformanceEstimationMethod _perfEstMethod,
			   bool _persistentAllocations,
			   int _iterationLatency,
			   Metric* _performanceMetric,
			   bool _enforcePolicy,
			   WriteStallTechnique _wst,
			   PrivBlockedStallTechnique _pbst,
			   EmptyROBStallTechnique _rst,
			   double _maximumDamping,
			   double _hybridDecisionError,
			   int _hybridBufferSize);

	~BasePolicy();

	void handlePolicyEvent();

	void handleTraceEvent();

	void registerCache(BaseCache* _cache, int _cpuID, int _maxMSHRs);
	void registerSharedCache(BaseCache* _cache);
	void registerBus(Bus* _bus);
	void registerFullCPU(FullCPU* _cpu, int _cpuID);

	void addTraceEntry(PerformanceMeasurement* measurement);

	virtual void runPolicy(PerformanceMeasurement measurements) = 0;
	virtual void prepareEstimates(){ }

	void doPerformanceModelTrace(int cpuID, PerformanceModelMeasurements modelMeasurements);

	void updatePrivPerfEst(int cpuID,
						   double avgSharedLat,
						   double avgPrivateLatEstimate,
						   int reqs,
						   int stallCycles,
						   int totalCycles,
						   int committedInsts,
						   int commitCycles,
						   Tick privateStallCycles,
						   double avgPrivateMemsysLat,
						   Tick writeStall,
						   int hiddenLoads,
						   Tick memoryIndependentStalls,
						   OverlapStatistics ols,
						   double privateMissRate,
						   double cwp,
						   double privateBlockedStall,
						   double avgSharedStoreLat,
						   double avgPrivmodeStoreLat,
						   double numStores,
						   int numWriteStalls,
						   int emptyROBStallCycles,
						   Tick boisAloneStallEst,
						   CacheAccessMeasurement privateLLCEstimates,
						   CacheAccessMeasurement sharedLLCMeasurements);

	static RequestEstimationMethod parseRequestMethod(std::string methodName);
	static PerformanceEstimationMethod parsePerformanceMethod(std::string methodName);
	static SearchAlgorithm parseSearchAlgorithm(std::string methodName);
	static Metric* parseOptimizationMetric(std::string metricName);
	static WriteStallTechnique parseWriteStallTech(std::string techName);
	static PrivBlockedStallTechnique parsePrivBlockedStallTech(std::string techName);
	static EmptyROBStallTechnique parseEmptyROBStallTech(std::string techName);

	void implementMHA(std::vector<int> bestMHA);

	virtual bool doEvaluation(int cpuID) = 0;

	virtual void initPolicy();
};

class MissBandwidthPolicyEvent : public Event{
private:
	BasePolicy* policy;
	Tick period;

public:
	MissBandwidthPolicyEvent(BasePolicy* _policy, Tick _period):
	Event(&mainEventQueue), policy(_policy), period(_period){

	}
	void process() {
		policy->handlePolicyEvent();
		schedule(curTick + period);
	}

	virtual const char *description() {
		return "Miss Bandwidth Policy Event";
	}

};

class MissBandwidthTraceEvent : public Event{
private:
	BasePolicy* policy;
	Tick period;

public:
	MissBandwidthTraceEvent(BasePolicy* _policy, Tick _period):
	Event(&mainEventQueue), policy(_policy), period(_period){

	}
	void process() {
		policy->handleTraceEvent();
		schedule(curTick + period);
	}

	virtual const char *description() {
		return "Miss Bandwidth Trace Event";
	}

};

class MissBandwidthImplementMHAEvent : public Event{

private:
	BasePolicy* policy;
	std::vector<int> bestMHA;

public:
	MissBandwidthImplementMHAEvent(BasePolicy* _policy, std::vector<int> _bestMHA):
		Event(&mainEventQueue), policy(_policy), bestMHA(_bestMHA){

	}

	void process(){
		policy->implementMHA(bestMHA);
		assert(!scheduled());
		delete this;
	}

	virtual const char *description() {
		return "Miss Bandwidth Implement MHA Event";
	}
};

class BasePolicyInitEvent : public Event{

private:
	BasePolicy* policy;

public:
	BasePolicyInitEvent(BasePolicy* _policy):
		Event(&mainEventQueue), policy(_policy){

	}

	void process(){
		policy->initPolicy();
		assert(!scheduled());
		delete this;
	}

	virtual const char *description() {
		return "Base Policy Initialization Event";
	}
};

class PolicyCallback : public Callback
{
    private:
	BasePolicy *bp;
    public:
        PolicyCallback(BasePolicy *p) : bp(p) {}
        virtual void process() { bp->handlePolicyEvent(); }
};

#endif /* BASE_POLICY_HH_ */
