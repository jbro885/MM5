
#ifndef __ADAPTIVE_MHA_HH__
#define __ADAPTIVE_MHA_HH__

#include "sim/sim_object.hh"
#include "mem/cache/base_cache.hh"
#include "mem/bus/bus.hh"
#include "mem/mem_req.hh"
#include "sim/eventq.hh"

class AdaptiveMHASampleEvent;

class AdaptiveMHA : public SimObject{
    
    private:
        
        int adaptiveMHAcpuCount;
        Tick sampleFrequency;
        double highThreshold;
        double lowThreshold;
        int neededRepeatDecisions;
        
        int maxMshrs;
        
        std::vector<BaseCache* > dataCaches;
        std::vector<BaseCache* > instructionCaches;
        
        std::vector<int> staticAsymmetricMHAs;
        
        int currentCandidate;
        int numRepeatDecisions;
        
        Bus* bus;
        
        AdaptiveMHASampleEvent* sampleEvent;
        
        std::string memTraceFileName;
        std::string adaptiveMHATraceFileName;
        
        bool firstSample;
        
        bool onlyTraceBus;
        
//         std::vector<int> zeroCount;
    
    public:
        
        Stats::Scalar<> dataSampleTooLarge;
        Stats::Scalar<> addrSampleTooLarge;
        
        AdaptiveMHA(const std::string &name,
                    double _lowThreshold,
                    double _highThreshold,
                    int cpu_count,
                    Tick _sampleFrequency,
                    Tick _startTick,
                    bool _onlyTraceBus,
                    int _neededRepeatDecisions,
                    std::vector<int> & _staticAsymmetricMHA);
        
        ~AdaptiveMHA();
        
        void regStats();
        
        void registerCache(int cpu_id, bool isDataCache, BaseCache* cache);
        
        void registerBus(Bus* _bus){
            bus = _bus;
            assert(bus != NULL);
        }
        
        void handleSampleEvent(Tick time);
        
        int getCPUCount(){
            return adaptiveMHAcpuCount;
        }
        
        int getSampleSize(){
            return sampleFrequency;
        }
        
    private:
        void decreaseNumMSHRs(std::vector<int> currentVector);
        
        void increaseNumMSHRs();

};

class AdaptiveMHASampleEvent : public Event
{

    public:
        
        AdaptiveMHA* adaptiveMHA;
        
        AdaptiveMHASampleEvent(AdaptiveMHA* _adaptiveMHA)
            : Event(&mainEventQueue), adaptiveMHA(_adaptiveMHA)
        {
        }
        
        void process(){
          //FIXME: this call causes a segfault, update needed
          //adaptiveMHA->handleSampleEvent(this->when());
        }

        virtual const char *description(){
            return "AdaptiveMHASampleEvent";
        }
};

#endif //__ADAPTIVE_MHA_HH__
