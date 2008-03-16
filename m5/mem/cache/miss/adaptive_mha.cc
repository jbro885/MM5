
#include "sim/builder.hh"
#include "adaptive_mha.hh"

#include <fstream>

// #define FATAL_ZERO_NUM 100

using namespace std;

AdaptiveMHA::AdaptiveMHA(const std::string &name,
                         double _lowThreshold,
                         double _highThreshold,
                         int cpu_count,
                         Tick _sampleFrequency,
                         Tick _startTick,
                         bool _onlyTraceBus,
                         int _neededRepeatDecisions,
                         vector<int> & _staticAsymmetricMHA)
        : SimObject(name)
{
    adaptiveMHAcpuCount = cpu_count;
    sampleFrequency = _sampleFrequency;
    highThreshold = _highThreshold;
    lowThreshold = _lowThreshold;
    
    onlyTraceBus = _onlyTraceBus;
    
    neededRepeatDecisions = _neededRepeatDecisions;
    
    currentCandidate = -1;
    numRepeatDecisions = 0;
    
    Tick start = _startTick;
    if(start % sampleFrequency != 0){
        start = start + (sampleFrequency - (start % sampleFrequency));
    }
    assert(start % sampleFrequency == 0);
    
    firstSample = true;
            
    maxMshrs = -1;
    
    assert(_staticAsymmetricMHA.size() == cpu_count);
    bool allStaticM1 = true;
    for(int i=0;i<_staticAsymmetricMHA.size();i++) if(_staticAsymmetricMHA[i] != -1) allStaticM1 = false;
    
    if(!allStaticM1) staticAsymmetricMHAs = _staticAsymmetricMHA;

    
    dataCaches.resize(adaptiveMHAcpuCount, NULL);
    instructionCaches.resize(adaptiveMHAcpuCount, NULL);
    
//     zeroCount.resize(adaptiveMHAcpuCount, 0);
            
    sampleEvent = new AdaptiveMHASampleEvent(this);
    sampleEvent->schedule(start);
    
    if(!onlyTraceBus){
        adaptiveMHATraceFileName = "adaptiveMHATrace.txt";
        ofstream mhafile(adaptiveMHATraceFileName.c_str());
        mhafile << "Tick";
        for(int i=0;i<cpu_count;i++) mhafile << ";DCache " << i << " MSHRs";
        for(int i=0;i<cpu_count;i++) mhafile << ";ICache " << i << " MSHRs";
        mhafile << "\n";
        mhafile.flush();
        mhafile.close();
    }
    
    memTraceFileName = "memoryBusTrace.txt";
    ofstream memfile(memTraceFileName.c_str());
    memfile << "Tick;Address Util;DataUtil";
    for(int i=0;i<cpu_count;i++) memfile << ";Cache " << i << " addr";
    for(int i=0;i<cpu_count;i++) memfile << ";Cache " << i << " data";
    memfile << "\n";
    memfile.flush();
    memfile.close();
}
        
AdaptiveMHA::~AdaptiveMHA(){
    delete sampleEvent;
}

void
AdaptiveMHA::regStats(){
    
    dataSampleTooLarge
            .name(name() + ".data_sample_too_large")
            .desc("Number of samples where there data sample was to large")
            ;
    
    addrSampleTooLarge
            .name(name() + ".addr_sample_too_large")
            .desc("Number of samples where there address sample was to large")
            ;
}

void
AdaptiveMHA::registerCache(int cpu_id, bool isDataCache, BaseCache* cache){
    
    if(isDataCache){
        assert(dataCaches[cpu_id] == NULL);
        dataCaches[cpu_id] = cache;
    }
    else{
        assert(instructionCaches[cpu_id] == NULL);
        instructionCaches[cpu_id] = cache;
    }
    
    if(!onlyTraceBus){
        if(maxMshrs == -1) maxMshrs = cache->getCurrentMSHRCount();
        else assert(maxMshrs == cache->getCurrentMSHRCount());
    }
}

void
AdaptiveMHA::handleSampleEvent(Tick time){
    
    
    if(firstSample){
        bus->resetAdaptiveStats();
        firstSample = false;
        
        if(staticAsymmetricMHAs.size() != 0){
            assert(onlyTraceBus);
            for(int i=0;i<staticAsymmetricMHAs.size();i++){
                for(int j=0;j<staticAsymmetricMHAs[i];j++){
                    dataCaches[i]->decrementNumMSHRs();
                }
                cout << curTick << ": The MSHR count of cache " << i << " has been reduced to " << dataCaches[i]->getCurrentMSHRCount() << "\n";
            }
        }
    }
    
    
    // gather information
    double addrBusUtil = bus->getAddressBusUtilisation(sampleFrequency);
    double dataBusUtil = bus->getDataBusUtilisation(sampleFrequency);
    
    vector<int> dataUsers = bus->getDataUsePerCPUId();
    assert(dataUsers.size() == adaptiveMHAcpuCount);
    vector<int> addressUsers = bus->getAddressUsePerCPUId();
    assert(addressUsers.size() == adaptiveMHAcpuCount);
    
    // Writeback requests are not initiated by the CPU
    int dataSum=0;
    for(int i=0;i<dataUsers.size();i++) dataSum += dataUsers[i];
//     cout << curTick << ": assert " << dataSum << " / " << sampleFrequency << " <= " << dataBusUtil << "\n";
    if((double) ((double) dataSum / (double) sampleFrequency) > dataBusUtil){
        dataSampleTooLarge++;
    }
    
    int addrSum=0;
    for(int i=0;i<addressUsers.size();i++) addrSum += addressUsers[i];
    if((double) ((double) addrSum / (double) sampleFrequency) > addrBusUtil){
        addrSampleTooLarge++;
    }
    
    assert(dataUsers.size() == addressUsers.size());
    
    //write bus use trace
    ofstream memTraceFile(memTraceFileName.c_str(), ofstream::app);
    memTraceFile << time;
    memTraceFile << ";" << addrBusUtil;
    memTraceFile << ";" << dataBusUtil;
    for(int i=0;i<addressUsers.size();i++) memTraceFile << ";" << addressUsers[i];
    for(int i=0;i<dataUsers.size();i++) memTraceFile << ";" << dataUsers[i];
    memTraceFile << "\n";
    
    memTraceFile.flush();
    memTraceFile.close();
    
    
    if(!onlyTraceBus){
    
        bool decreaseCalled = false;
        if(dataBusUtil >= highThreshold){
            decreaseNumMSHRs(dataUsers);
            decreaseCalled = true;
        }
        if(dataBusUtil <= lowThreshold){
            increaseNumMSHRs();
        }
        
        if(!decreaseCalled){
            numRepeatDecisions = 0;
            currentCandidate = -1;
        }
        
        // write Adaptive MHA trace file
        ofstream mhafile(adaptiveMHATraceFileName.c_str(), ofstream::app);
        mhafile << time;
        for(int i=0;i<dataCaches.size();i++) mhafile << ";" << dataCaches[i]->getCurrentMSHRCount();
        for(int i=0;i<instructionCaches.size();i++) mhafile << ";" << instructionCaches[i]->getCurrentMSHRCount();
        mhafile << "\n";
        mhafile.flush();
        mhafile.close();
    }
        
    assert(!sampleEvent->scheduled());
    sampleEvent->schedule(time + sampleFrequency);
}

void
AdaptiveMHA::decreaseNumMSHRs(vector<int> currentVector){
    
    int index = -1;
    int largest = 0;
    for(int i=0;i<currentVector.size();i++){
        if(currentVector[i] > largest && dataCaches[i]->getCurrentMSHRCount() > 1){
            largest = currentVector[i];
            index = i;
        }
    }
    
    if(index == -1){
        // we have reduced all bus users to blocking cache configurations
        // the others do not use the bus, return
        return;
    }
    
    assert(index > -1);

    if(index == currentCandidate){
        
        numRepeatDecisions++;
        if(numRepeatDecisions >= neededRepeatDecisions){
            dataCaches[index]->decrementNumMSHRs();
            numRepeatDecisions = 0;
        }
    }
    else{

        currentCandidate = index;
        numRepeatDecisions = 1;
        
        if(neededRepeatDecisions == 1){
            dataCaches[index]->decrementNumMSHRs();
            numRepeatDecisions = 0;
        }

    }
    return;
}

void
AdaptiveMHA::increaseNumMSHRs(){
    
    int smallest = 100;
    int index = -1;
    
    for(int i=0;i<adaptiveMHAcpuCount;i++){
        if(dataCaches[i]->getCurrentMSHRCount() < smallest){
            smallest = dataCaches[i]->getCurrentMSHRCount();
            index = i;
        }
    }
    
    assert(index > -1);
    if(smallest == maxMshrs) return;
    
    // reset the decrease stats
    currentCandidate = -1;
    numRepeatDecisions = 0;
    
    // no filtering here, we should increase quickly
    dataCaches[index]->incrementNumMSHRs();
    
    // Unblock the cache if it is blocked due to too few MSHRs
    if(dataCaches[index]->isBlockedNoMSHRs()){
        assert(dataCaches[index]->isBlocked());
        dataCaches[index]->clearBlocked(Blocked_NoMSHRs);
    }
}


#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(AdaptiveMHA)
    Param<double> lowThreshold;
    Param<double> highThreshold;
    Param<int> cpuCount;
    Param<Tick> sampleFrequency;
    Param<Tick> startTick;
    Param<bool> onlyTraceBus;
    Param<int> neededRepeats;
    VectorParam<int> staticAsymmetricMHA;
END_DECLARE_SIM_OBJECT_PARAMS(AdaptiveMHA)

BEGIN_INIT_SIM_OBJECT_PARAMS(AdaptiveMHA)
    INIT_PARAM(lowThreshold, "Threshold for increasing the number of MSHRs"),
    INIT_PARAM(highThreshold, "Threshold for reducing the number of MSHRs"),
    INIT_PARAM(cpuCount, "Number of cores in the CMP"),
    INIT_PARAM(sampleFrequency, "The number of clock cycles between each sample"),
    INIT_PARAM(startTick, "The tick where the scheme is started"),
    INIT_PARAM(onlyTraceBus, "Only create the bus trace, adaptiveMHA is turned off"),
    INIT_PARAM(neededRepeats, "Number of repeated desicions to change config"),
    INIT_PARAM(staticAsymmetricMHA, "The number of times each caches mshrcount should be reduced")
END_INIT_SIM_OBJECT_PARAMS(AdaptiveMHA)

CREATE_SIM_OBJECT(AdaptiveMHA)
{
    return new AdaptiveMHA(getInstanceName(),
                           lowThreshold,
                           highThreshold,
                           cpuCount,
                           sampleFrequency,
                           startTick,
                           onlyTraceBus,
                           neededRepeats,
                           staticAsymmetricMHA);
}

REGISTER_SIM_OBJECT("AdaptiveMHA", AdaptiveMHA)

#endif //DOXYGEN_SHOULD_SKIP_THIS

