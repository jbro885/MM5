
#include "fcfs_interference.hh"

using namespace std;

FCFSControllerInterference::FCFSControllerInterference(const std::string& _name,
													   int _cpu_cnt,
													   TimingMemoryController* _ctrl)
:ControllerInterference(_name,_cpu_cnt,_ctrl)
{
	privateStorageInited = false;

	privateRequestQueues.resize(_cpu_cnt, vector<MemReqPtr>());
}

void
FCFSControllerInterference::insertRequest(MemReqPtr& req){
	assert(req->interferenceMissAt == 0);

	if(!privateStorageInited){
		privateStorageInited = true;
		initializeMemDepStructures(memoryController->getMemoryInterface()->getMemoryBankCount());
	}

	assert(req->adaptiveMHASenderID != -1);
	int fromCPU = req->adaptiveMHASenderID;
	int privateLatencyEstimate = 0;
	int privateQueueEstimate = 0;

	estimatePageResult(req);

	//TODO: update estimations with FCFS-numbers
    if(req->privateResultEstimate == DRAM_RESULT_HIT){
    	estimatedNumberOfHits[fromCPU]++;
        privateLatencyEstimate = 40;
    }
    else if(req->privateResultEstimate == DRAM_RESULT_CONFLICT){
    	estimatedNumberOfConflicts[fromCPU]++;
        if(req->cmd == Read) privateLatencyEstimate = 191;
        else privateLatencyEstimate = 184;
    }
    else{
    	assert(req->privateResultEstimate == DRAM_RESULT_MISS);
    	estimatedNumberOfMisses[fromCPU]++;
        if(req->cmd == Read) privateLatencyEstimate = 120;
        else privateLatencyEstimate = 109;
    }

    for(int i=0;i<privateRequestQueues[fromCPU].size();i++){
    	privateQueueEstimate += privateRequestQueues[fromCPU][i]->busAloneServiceEstimate;
    }

    privateRequestQueues[fromCPU].push_back(req);
    req->busAloneServiceEstimate = privateLatencyEstimate;
    req->busAloneReadQueueEstimate = privateQueueEstimate;
}

void
FCFSControllerInterference::estimatePrivateLatency(MemReqPtr& req){
	assert(req->adaptiveMHASenderID != -1);
	privateRequestQueues[req->adaptiveMHASenderID]
	                     .erase(privateRequestQueues[req->adaptiveMHASenderID].begin());
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(FCFSControllerInterference)
	SimObjectParam<TimingMemoryController*> memory_controller;
    Param<int> cpu_count;
END_DECLARE_SIM_OBJECT_PARAMS(FCFSControllerInterference)

BEGIN_INIT_SIM_OBJECT_PARAMS(FCFSControllerInterference)
	INIT_PARAM_DFLT(memory_controller, "Associated memory controller", NULL),
    INIT_PARAM_DFLT(cpu_count, "number of cpus",-1)
END_INIT_SIM_OBJECT_PARAMS(FCFSControllerInterference)

CREATE_SIM_OBJECT(FCFSControllerInterference)
{
    return new FCFSControllerInterference(getInstanceName(),
										  cpu_count,
										  memory_controller);
}

REGISTER_SIM_OBJECT("FCFSControllerInterference", FCFSControllerInterference)

#endif
