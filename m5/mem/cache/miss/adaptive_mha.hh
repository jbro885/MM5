
#ifndef __ADAPTIVE_MHA_HH__
#define __ADAPTIVE_MHA_HH__

#include "sim/sim_object.hh"
#include "mem/cache/base_cache.hh"
#include "cpu/base.hh"
#include "encumbered/cpu/full/cpu.hh"
#include "mem/bus/bus.hh"
#include "mem/mem_req.hh"
#include "sim/eventq.hh"
#include "mem/interconnect/interconnect.hh"

#include <fstream>

class BaseCache;
class AdaptiveMHASampleEvent;

enum InterferenceType{
    INTERCONNECT_INTERFERENCE,
    L2_INTERFERENCE,
    MEMORY_SERIALIZATION_INTERFERENCE,
    MEMORY_BLOCKED_INTERFERENCE,
    MEMORY_PRIVATE_BLOCKED_INTERFERENCE,
    L2_CAPACITY_INTERFERENCE,
    MEMORY_INTERFERENCE, // not used in new interference measurements
    INTERFERENCE_COUNT
};

class AdaptiveMHA : public SimObject{

    private:

        int adaptiveMHAcpuCount;
        Tick sampleFrequency;
        double highThreshold;
        double lowThreshold;
        int neededRepeatDecisions;

        bool useFairAMHA;
        bool useAvgLat;
        bool useTwoPhaseThroughput;

        int maxMshrs;
        int maxWB;

        std::vector<BaseCache* > dataCaches;
        std::vector<BaseCache* > instructionCaches;
        std::vector<BaseCache* > sharedCaches;

        std::vector<int> staticAsymmetricMHAs;

        int currentCandidate;
        int numRepeatDecisions;

        Bus* bus;
        Interconnect* interconnect;

        AdaptiveMHASampleEvent* sampleEvent;

//        std::string memTraceFileName;
//        std::string adaptiveMHATraceFileName;
//        std::string ipcTraceFileName;
//        std::string aloneInterferenceFileName;

        bool firstSample;

        bool onlyTraceBus;

        std::vector<FullCPU* > cpus;

        std::vector<std::vector<Tick> > totalInterferenceDelayRead;
        std::vector<std::vector<Tick> > totalInterferenceDelayWrite;
        std::vector<Tick> totalSharedDelay;
        std::vector<Tick> totalSharedWritebackDelay;
        int interferenceOverflow;

        int numInterferenceRequests;
        int numDelayRequests;
        std::vector<int> delayReadRequestsPerCPU;
        std::vector<int> delayWriteRequestsPerCPU;

        std::vector<std::vector<bool> > interferenceBlacklist;
        int lastInterfererID;
        int lastVictimID;
        double lastInterferenceValue;
        Tick lastInterferenceValueTick;

        int resetCounter;
        int localResetCounter;
        double reductionThreshold;
        double interferencePointMinAllowed;

        bool printInterference;
        Tick finalSimTick;



        struct delayEntry{
            std::vector<std::vector<Tick> > cbDelay;
            std::vector<std::vector<Tick> > l2Delay;
            std::vector<std::vector<Tick> > memDelay;
            Tick totalDelay;
            bool isRead;

            delayEntry(){
                totalDelay = 0;
                isRead = true;
            }

            delayEntry(const delayEntry &entry){
                cbDelay = entry.cbDelay;
                l2Delay = entry.l2Delay;
                memDelay = entry.memDelay;
                totalDelay = entry.totalDelay;
                isRead = entry.isRead;
            }

            delayEntry(std::vector<std::vector<Tick> > _delays, bool read, InterferenceType type);

            void addDelays(std::vector<std::vector<Tick> > newDelays, InterferenceType type);

            bool committed(){
                return (totalDelay != 0);
            }
        };

        std::map<Addr,delayEntry> oracleStorage;

        Tick startRunning;
        int dumpAtNumReqs;

        std::vector<Tick> busInterference;
        std::vector<Tick> busBlockedInterference;
        std::vector<Tick> busPrivateBlockedInterference;
        std::vector<Tick> l2CapInterference;
        std::vector<Tick> l2BwInterference;
        std::vector<Tick> interconnectInterference;

        std::vector<Tick> totalAloneDelay;
        std::vector<int> numberOfAloneRequests;
        std::vector<int> dumpCount;

        // Two phase AMHA vars
        std::vector<double> lastIPCs;
        int lastDesicionID;
        bool lastWasDecrease;
        bool inPhaseOne;
        int twoPhaseResetCount;
        std::vector<bool> tfamhaBlacklist;
        bool useAMHAInSession;
        double tpUtilizationLimit;

        int firstPhaseStep;
        std::vector<double> throughputMeasurements;
        std::vector<std::vector<double> > speedDifferences;
        std::vector<int> stepMapping;
        bool firstPhaseTwo;
        std::vector<int> performanceOrdering;


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
                    std::vector<int> & _staticAsymmetricMHA,
                    bool _useFairAMHA,
                    int _resetCounter,
                    double _reductionThreshold,
                    double _interferencePointMinAllowed,
                    bool _printInterference,
                    Tick _finalSimTick,
                    int _numReqsBetweenIDumps,
                    double _tpUtilizationLimit);

        ~AdaptiveMHA();

        void regStats();

        void registerCache(int cpu_id, bool isDataCache, BaseCache* cache, bool isShared);

        void registerBus(Bus* _bus){
            bus = _bus;
            assert(bus != NULL);
        }

        void registerInterconnect(Interconnect* _interconnect){
            interconnect = _interconnect;
            assert(interconnect != NULL);
        }

        void handleSampleEvent(Tick time);

        void handleInterferenceDumpEvent();

        int getCPUCount(){
            return adaptiveMHAcpuCount;
        }

        int getSampleSize(){
            return sampleFrequency;
        }

        void registerFullCPU(int id, FullCPU* cpu){
            assert(id < cpus.size());
            assert(cpus[id] == 0);
            cpus[id] = cpu;
        }

        void addInterferenceDelay(std::vector<std::vector<Tick> > perCPUQueueTimes,
                                  Addr addr,
                                  MemCmd cmd,
                                  int fromCPU,
                                  InterferenceType type,
                                  std::vector<std::vector<bool> > nextIsRead);
        void addTotalDelay(int issuedCPU, Tick delay, Addr addr, bool isRead);

        void coreCommittedInstruction(int cpuID);

        bool requestInROB(MemReqPtr& req, int cpuID, int blockSize);

    private:

        void doFairAMHA();

        void doThroughputAMHA(double dataBusUtil, std::vector<int> dataUsers, std::vector<double> avgQueueLatencies);

        void doTwoPhaseThroughputAMHA(std::vector<int> dataUsers, std::vector<double> avgQueueLatencies, std::vector<double> curIPCs, double dataBusUtil);

        int findMaxNotBlacklisted(std::vector<int> dataUsers);

        void getPhaseTwoAction(std::vector<double> avgLatencies, int* id, bool* increase, double dataBusUtil);

        bool acceptSpeedup(std::vector<double> aCurIPCs, std::ofstream &amhatrace);

        void changeNumMSHRs(int id, bool increment, bool exponential);

        void throughputDecreaseNumMSHRs(std::vector<int> currentVector);

        void throughputIncreaseNumMSHRs();

        void fairAMHAFirstAlg(std::ofstream& fairfile,
                              std::vector<std::vector<double> >& relativeInterferencePoints,
                              std::vector<Tick>& numReads,
                              std::vector<Tick>& numWrites,
                              std::vector<int>& stalledCycles,
                              double maxDifference,
                              Tick lowestAccStallTime);

        void maxDiffRedWithRollback(std::ofstream& fairfile,
                                    std::vector<std::vector<double> >& relativeInterferencePoints,
                                    std::vector<Tick>& numReads,
                                    std::vector<int>& stalledCycles,
                                    double maxDifference);

        void IPDirectWithRollback(std::ofstream& fairfile,
                                  std::vector<std::vector<Tick> >& readInterference,
                                  std::vector<int>& sharedCacheCapacityIPs,
                                  std::vector<Tick>& numReads,
                                  std::vector<int>& stalledCycles,
                                  double maxDifference);

        void dampingAndCacheAnalysis(std::ofstream& fairfile,
                                     std::vector<std::vector<Tick> >& readInterference,
                                     std::vector<int>& sharedCacheCapacityIPs,
                                     std::vector<Tick>& numReads,
                                     std::vector<int>& stalledCycles,
                                     double maxDifference);

        template <class T> void printMatrix(std::vector<std::vector<T> >& matrix, std::ofstream &file, std::string header);
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
          adaptiveMHA->handleSampleEvent(this->when());
        }

        virtual const char *description(){
            return "AdaptiveMHASampleEvent";
        }
};

class AdaptiveMHADumpInterferenceEvent : public Event
{

    public:

        AdaptiveMHA* adaptiveMHA;

        AdaptiveMHADumpInterferenceEvent(AdaptiveMHA* _adaptiveMHA)
            : Event(&mainEventQueue), adaptiveMHA(_adaptiveMHA)
        {
        }

        void process(){
            adaptiveMHA->handleInterferenceDumpEvent();
            delete this;
        }

        virtual const char *description(){
            return "AdaptiveMHADumpInterferenceEvent";
        }
};

#endif //__ADAPTIVE_MHA_HH__
