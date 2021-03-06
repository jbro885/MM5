
#include "sim/builder.hh"
#include "ring.hh"

using namespace std;

#define CHECK_RING_ORDERING

Ring::Ring(const std::string &_name,
                               int _width,
                               int _clock,
                               int _transDelay,
                               int _arbDelay,
                               int _cpu_count,
                               HierParams *_hier,
                               AdaptiveMHA* _adaptiveMHA,
                               Tick _detailedStart,
                               int _singleProcessorID,
							   InterferenceManager* _intman,
							   int _fixedRoundtripLat)
    : AddressDependentIC(_name,
                         _width,
                         _clock,
                         _transDelay,
                         _arbDelay,
                         _cpu_count,
                         _hier,
                         _adaptiveMHA,
                         _intman)
{

    sharedCacheBankCount = 4; //FIXME: parameterize
    queueSize = 32; // FIXME: parameterize

    fixedRoundtripLat = _fixedRoundtripLat;

    if(_cpu_count == 4 || _cpu_count == 2){
        numberOfRequestRings = 1;
    }
    else if(_cpu_count == 8){
        numberOfRequestRings = 2;
    }
    else if(_cpu_count == 16){
        numberOfRequestRings = 2;
    }
    else{
        fatal("Ring parameters not known for supplied CPU-count");
    }

    numberOfResponseRings = 1;

    detailedSimStartTick = _detailedStart;

    singleProcessorID = _singleProcessorID;

    initQueues(_cpu_count,_cpu_count + sharedCacheBankCount);

    recvBufferSize = (_cpu_count/2) + (sharedCacheBankCount/2);

    ringRequestQueue.resize(_cpu_count, list<RingRequestEntry>());
    ringResponseQueue.resize(sharedCacheBankCount, list<RingRequestEntry>());
    deliverBuffer.resize(sharedCacheBankCount, list<RingRequestEntry>());

    inFlightRequests.resize(sharedCacheBankCount, 0);

    assert(_arbDelay != -1);
    assert(_transDelay % _arbDelay == 0);

    arbEvent = new ADIArbitrationEvent(this);

    TOP_LINK_ID = 2000;
    BOTTOM_LINK_ID = 3000;

    ringLinkOccupied.resize(numberOfRequestRings + numberOfResponseRings, map<int,vector<list<pair<Tick,RingRequestEntry> > > >());

    // initialize occupation storage
    for(int r=0;r<numberOfRequestRings+numberOfResponseRings;r++){
        for(int i=0;i<_cpu_count-1;i++){
            ringLinkOccupied[r][i].resize(2, list<pair<Tick, RingRequestEntry> >());
        }
        for(int i=0;i<sharedCacheBankCount-1;i++){
            ringLinkOccupied[r][_cpu_count+i].resize(2,list<pair<Tick, RingRequestEntry> >());
        }
        ringLinkOccupied[r][TOP_LINK_ID].resize(2,list<pair<Tick, RingRequestEntry> >());
        ringLinkOccupied[r][BOTTOM_LINK_ID].resize(2,list<pair<Tick, RingRequestEntry> >());
    }
}

int
Ring::registerInterface(InterconnectInterface* interface, bool isSlave, int processorID){

    int interfaceID = -1;

    if(singleProcessorID != -1 && !isSlave){
        interfaceID = Interconnect::registerInterface(interface,isSlave,-1);

        processorIDToInterconnectIDs[singleProcessorID].push_back(interfaceID);
        for(int i=0;i<processorIDToInterconnectIDs.size();i++){
            if(i != singleProcessorID) assert(processorIDToInterconnectIDs[i].empty());
        }

        assert(interconnectIDToProcessorIDMap.find(interfaceID) == interconnectIDToProcessorIDMap.end());
        interconnectIDToProcessorIDMap.insert(make_pair(interfaceID, singleProcessorID));
    }
    else{
        interfaceID = Interconnect::registerInterface(interface,isSlave,processorID);
    }

    assert(interfaceID != -1);
    return interfaceID;
}

void
Ring::send(MemReqPtr& req, Tick time, int fromID){

    req->inserted_into_crossbar = curTick;

    if(singleProcessorID != -1 && allInterfaces[fromID]->isMaster()){
        assert(req->interferenceAccurateSenderID == singleProcessorID);
    }

    if(curTick < detailedSimStartTick){
        // infinite bw in warm-up, deliver request directly
        if(allInterfaces[fromID]->isMaster()){
            ADIDeliverEvent* delivery = new ADIDeliverEvent(this, req, true);
            setDestinationIntID(req, fromID);
            int toSlaveID = interconnectIDToL2IDMap[req->toInterfaceID];
            inFlightRequests[toSlaveID]++;
            delivery->schedule(curTick + arbitrationDelay);
        }
        else{
            ADIDeliverEvent* delivery = new ADIDeliverEvent(this, req, false);
            delivery->schedule(curTick + arbitrationDelay);
        }
        return;
    }

    if(fixedRoundtripLat != -1){
    	createFixedLatencyResponse(fixedRoundtripLat, fromID, req);
    	return;
    }

    updateEntryInterference(req, fromID);

    if(allInterfaces[fromID]->isMaster()){

        RING_DIRECTION direction = RING_CLOCKWISE;
        vector<int> resourceReq = findResourceRequirements(req, fromID, &direction);
        assert(direction != -1);

        assert(req->interferenceAccurateSenderID != -1);
        ringRequestQueue[req->interferenceAccurateSenderID].push_back(RingRequestEntry(req, curTick, resourceReq, direction));
        if(ringRequestQueue[req->interferenceAccurateSenderID].size() == queueSize){
            setBlockedLocal(req->interferenceAccurateSenderID);
        }
        assert(ringRequestQueue[req->interferenceAccurateSenderID].size() <= queueSize);
    }
    else{
        RING_DIRECTION direction = RING_CLOCKWISE;
        vector<int> resourceReq = findResourceRequirements(req,fromID, &direction);

        int slaveID = interconnectIDToL2IDMap[fromID];
        ringResponseQueue[slaveID].push_back(RingRequestEntry(req, curTick, resourceReq, direction));
        assert(ringResponseQueue.size() <= queueSize);
    }

    attemptToScheduleArbEvent();
}

void
Ring::attemptToScheduleArbEvent(){
    if(!arbEvent->scheduled()){
        Tick nextTime = curTick + (arbitrationDelay - (curTick % arbitrationDelay));
        arbEvent->schedule(nextTime);
        DPRINTF(Crossbar,"Scheduling arbitration event for tick %d\n", nextTime);
    }
}

void
Ring::setDestinationIntID(MemReqPtr& req, int fromIntID){
    if(allInterfaces[fromIntID]->isMaster()){
        int toInterfaceID = -1;
        for(int i=0;i<slaveInterfaces.size();i++){
            if(slaveInterfaces[i]->inRange(req->paddr)){
                if(toInterfaceID != -1) fatal("two suppliers for same address in ring");
                toInterfaceID = L2IDMapToInterconnectID[i];
            }
        }
        req->toInterfaceID = toInterfaceID;
    }
}


int
Ring::getDownhops(int cpuID, int slaveID){
	return (cpu_count - (cpuID+1)) + (sharedCacheBankCount - (slaveID+1)) + 1;
}

int
Ring::getUphops(int cpuID, int slaveID){
	return cpuID + slaveID + 1;
}

vector<int>
Ring::findResourceRequirements(MemReqPtr& req, int fromIntID, RING_DIRECTION* direction){

    setDestinationIntID(req, fromIntID);

    vector<int> path;
    int slaveIntID = req->toInterfaceID;
    assert(slaveIntID != -1);
    int slaveID = interconnectIDToL2IDMap[slaveIntID];
    int uphops = getUphops(req->interferenceAccurateSenderID, slaveID);
    int downhops = getDownhops(req->interferenceAccurateSenderID, slaveID);

    if(!inSingleCoreMode()){
    	int baselineUphops = getUphops(0, slaveID);
    	int baselineDownhops = getDownhops(0, slaveID);
    	req->ringBaselineHops  = (baselineUphops > baselineDownhops ? baselineDownhops: baselineUphops);
    }

    if(allInterfaces[fromIntID]->isMaster()){
        path = findMasterPath(req, uphops, downhops, direction);
    }
    else{
        path = findSlavePath(req, uphops, downhops, direction);
    }

    stringstream pathstr;
    for(int i=0;i<path.size();i++) pathstr << path[i] << " ";

    DPRINTF(Crossbar, "Ring recieved req from icID %d, to ICID %d, proc %d, path: %s, uphops %d, downhops %d, %s\n",
    		allInterfaces[fromIntID]->isMaster() ? req->fromInterfaceID : req->toInterfaceID,
            allInterfaces[fromIntID]->isMaster() ? req->toInterfaceID : req->fromInterfaceID,
            req->interferenceAccurateSenderID,
            pathstr.str().c_str(),
            uphops,
            downhops,
            (*direction == RING_CLOCKWISE ? "clockwise" : "counterclockwise"));

    return path;
}

vector<int>
Ring::findMasterPath(MemReqPtr& req, int uphops, int downhops, RING_DIRECTION* direction){

    vector<int> path;
    assert(req->toInterfaceID != -1);
    int toSlaveID = interconnectIDToL2IDMap[req->toInterfaceID];
    assert(sharedCacheBankCount == slaveInterfaces.size());

    if(uphops <= downhops){
        *direction = RING_CLOCKWISE;
        for(int i=(req->interferenceAccurateSenderID-1);i>=0;i--) path.push_back(i);
        path.push_back(TOP_LINK_ID);
        for(int i=0;i<toSlaveID;i++) path.push_back(i+cpu_count);
    }
    else{
        *direction = RING_COUNTERCLOCKWISE;
        for(int i=req->interferenceAccurateSenderID;i<cpu_count-1;i++) path.push_back(i);
        path.push_back(BOTTOM_LINK_ID);
        for(int i=sharedCacheBankCount-2;i>=toSlaveID;i--) path.push_back(i+cpu_count);
    }

    return path;
}

vector<int>
Ring::findSlavePath(MemReqPtr& req, int uphops, int downhops, RING_DIRECTION* direction){

    vector<int> path;

    assert(req->toInterfaceID != -1);
    int slaveID = interconnectIDToL2IDMap[req->toInterfaceID];

    if(uphops <= downhops){
        *direction = RING_COUNTERCLOCKWISE;
        for(int i=(slaveID-1);i>=0;i--) path.push_back(i+cpu_count);
        path.push_back(TOP_LINK_ID);
        for(int i=0;i<req->interferenceAccurateSenderID;i++) path.push_back(i);
    }
    else{
        *direction = RING_CLOCKWISE;
        for(int i=slaveID;i<sharedCacheBankCount-1;i++) path.push_back(i+cpu_count);
        path.push_back(BOTTOM_LINK_ID);
        for(int i=cpu_count-2;i>=req->interferenceAccurateSenderID;i--) path.push_back(i);
    }

    return path;
}

vector<int>
Ring::findServiceOrder(vector<list<RingRequestEntry> >* queue){
    vector<int> order;
    order.resize(queue->size(), -1);

    vector<bool> marked;
    marked.resize(queue->size(), false);

    if(queue == &ringRequestQueue){
        DPRINTF(Crossbar, "Master Service order: ");
        assert(queue->size() == cpu_count);
    }
    else{
        DPRINTF(Crossbar, "Slave Service order: ");
        assert(queue->size() == sharedCacheBankCount);
    }
    for(int i=0;i<queue->size();i++){

        Tick min = 1000000000000000ull;
        Tick minIndex = -1;

        for(int j=0;j<queue->size();j++){
            if(!marked[j] && !(*queue)[j].empty()){
                if((*queue)[j].front().enteredAt < min){
                    min = (*queue)[j].front().enteredAt;
                    minIndex = j;
                }
            }
        }

        if(minIndex == -1){
            for(int j=0;j<queue->size();j++){
                if(!marked[j]){
                    assert((*queue)[j].empty());
                    minIndex = j;
                    break;
                }
            }
        }

        assert(minIndex != -1);
        marked[minIndex] = true;
        order[i] = minIndex;

        DPRINTFR(Crossbar, "%d:%d ", i, minIndex);
    }
    DPRINTFR(Crossbar, "\n");

    for(int i=0;i<order.size();i++) assert(order[i] >= 0 && order[i] < order.size());

    return order;
}

void
Ring::removeOldEntries(int ringID){
    map<int,vector<list<pair<Tick,RingRequestEntry> > > >::iterator remIt;
    for(remIt = ringLinkOccupied[ringID].begin();remIt != ringLinkOccupied[ringID].end();remIt++){
        for(int i=0;i<RING_DIRCOUNT;i++){
            if(!remIt->second[i].empty()){
                while(!remIt->second[i].empty() && remIt->second[i].front().first < curTick){
                    remIt->second[i].pop_front();
                }

                if(!remIt->second[i].empty()) assert(remIt->second[i].front().first >= curTick);
            }
        }
    }
}

bool
Ring::checkStateAndSend(RingRequestEntry entry, int ringID, bool toSlave){

    Tick transTick = curTick;
    for(int i=0;i<entry.resourceReq.size();i++){
        list<pair<Tick,RingRequestEntry> >::iterator checkIt;
        for(checkIt = ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].begin();
            checkIt != ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].end();
            checkIt++){

            if(checkIt->first == transTick){
                DPRINTF(Crossbar, "CPU %d not granted, conflict for link %d at %d\n",
                        entry.req->interferenceAccurateSenderID,
                        entry.resourceReq[i],
                        transTick);

                return false;
            }
        }
        transTick += transferDelay;
    }

    if(toSlave){
        int toSlaveID = interconnectIDToL2IDMap[entry.req->toInterfaceID];

        if(inFlightRequests[toSlaveID] == recvBufferSize){
            DPRINTF(Crossbar, "CPU %d not granted, %d reqs in flight to slave %d\n", entry.req->interferenceAccurateSenderID, inFlightRequests[toSlaveID], toSlaveID);
            return false;
        }
        else{
            inFlightRequests[toSlaveID]++;
        }
    }

    totalArbQueueCycles += curTick - entry.req->inserted_into_crossbar;
    arbitratedRequests++;

    // update state
    transTick = curTick;
    for(int i=0;i<entry.resourceReq.size();i++){
        list<pair<Tick,RingRequestEntry> >::iterator checkIt;
        bool inserted = false;
        for(checkIt = ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].begin();
            checkIt != ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].end();
            checkIt++){

            if(checkIt->first > transTick){
                ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].insert(checkIt, pair<Tick,RingRequestEntry>(transTick,entry));
                inserted = true;
                break;
            }
        }

        if(!inserted) ringLinkOccupied[ringID][entry.resourceReq[i]][entry.direction].push_back(pair<Tick,RingRequestEntry>(transTick,entry));
        transTick += transferDelay;
    }

    ADIDeliverEvent* delivery = new ADIDeliverEvent(this, entry.req, toSlave);

	entry.req->interconnectTransferDelay = transferDelay * entry.resourceReq.size();
    if(!inSingleCoreMode()){
    	assert(entry.req->ringBaselineHops != -1);
    	entry.req->ringBaselineTransLat = transferDelay * entry.req->ringBaselineHops;
    }
    else{
    	assert(entry.req->ringBaselineHops == -1);
    }

    delivery->schedule(curTick + (transferDelay * entry.resourceReq.size()));

    totalTransferCycles += (transferDelay * entry.resourceReq.size());
    sentRequests++;

    DPRINTF(Crossbar, "Granting access to CPU %d, from ICID %d, to IDID %d, latency %d, hops %d\n",
            entry.req->interferenceAccurateSenderID,
            entry.req->fromInterfaceID,
            entry.req->toInterfaceID,
            transferDelay * entry.resourceReq.size(),
            entry.resourceReq.size());

    return true;
}

void
Ring::checkRingOrdering(int ringID){
    map<int, vector<list<pair<Tick, RingRequestEntry> > > >::iterator mapIt;
    for(mapIt = ringLinkOccupied[ringID].begin();mapIt != ringLinkOccupied[ringID].end();mapIt++){
        for(int j=0;j<RING_DIRCOUNT;j++){
            list<pair<Tick, RingRequestEntry> > curList = mapIt->second[j];
            list<pair<Tick, RingRequestEntry> >::iterator checkIt;
            Tick current = curList.front().first;
            for(checkIt = curList.begin();checkIt != curList.end();checkIt++){
                assert(current <= checkIt->first);
                current = checkIt->first;
            }
        }
    }
}

void
Ring::arbitrate(Tick time){

    assert(curTick % arbitrationDelay == 0);

    DPRINTF(Crossbar, "Ring arbitrating\n");
    for(int i=0;i<numberOfRequestRings+numberOfResponseRings;i++) removeOldEntries(i);

#ifdef CHECK_RING_ORDERING
    for(int i=0;i<numberOfRequestRings+numberOfResponseRings;i++) checkRingOrdering(i);
#endif

    arbitrateRing(&ringRequestQueue,0,numberOfRequestRings, true);
    arbitrateRing(&ringResponseQueue,numberOfRequestRings,numberOfRequestRings+numberOfResponseRings, false);

    for(int i=0;i<ringRequestQueue.size();i++){
        if(ringRequestQueue[i].size() < queueSize && blockedLocalQueues[i]){
            clearBlockedLocal(i);
        }
    }

    retrieveAdditionalRequests();

    if(hasWaitingRequests()) attemptToScheduleArbEvent();
}

bool
Ring::hasWaitingRequests(){
    for(int i=0;i<ringRequestQueue.size();i++){
        if(!ringRequestQueue[i].empty()){
            DPRINTF(Crossbar, "CPU %d has a waiting request\n", i);
            return true;
        }
    }
    for(int i=0;i<ringResponseQueue.size();i++){
        if(!ringResponseQueue[i].empty()){
            DPRINTF(Crossbar, "Slave %d has a waiting request\n", i);
            return true;
        }
    }
    return false;
}

void
Ring::arbitrateRing(std::vector<std::list<RingRequestEntry> >* queue, int startRingID, int endRingID, bool toSlave){
    vector<int> order = findServiceOrder(queue);

    bool sent = true;
    int orderPos = 0;

    vector<bool> sentForCPUID(cpu_count,false);
    vector<bool> isDeliveryInterference(cpu_count,false);

    while(sent && orderPos < order.size()){
        sent = false;
        if(!(*queue)[order[orderPos]].empty()){
            for(int j=startRingID;j<endRingID;j++){
                sent = checkStateAndSend((*queue)[order[orderPos]].front(), j, toSlave);
                if(sent) break;
            }

            if(sent){
            	sentForCPUID[(*queue)[order[orderPos]].front().req->interferenceAccurateSenderID] = true;
                (*queue)[order[orderPos]].pop_front();
                orderPos++;
            }
            else{
                if(toSlave){
                    int toSlaveID = interconnectIDToL2IDMap[(*queue)[order[orderPos]].front().req->toInterfaceID];
                    if(inFlightRequests[toSlaveID] == recvBufferSize){

                    	isDeliveryInterference[(*queue)[order[orderPos]].front().req->interferenceAccurateSenderID] = true;
                        list<RingRequestEntry>::iterator intIt = (*queue)[order[orderPos]].begin();
                        for( ; intIt != (*queue)[order[orderPos]].end() ; intIt ++){
                            intIt->req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] -= arbitrationDelay;
                            intIt->req->latencyBreakdown[INTERCONNECT_DELIVERY_LAT] += arbitrationDelay;

                            //TODO: this code assumes all delivery latency is interference
                            intIt->req->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT] += arbitrationDelay;

                            if(intIt->req->cmd == Read){

                            	interferenceManager->addLatency(InterferenceManager::InterconnectRequestQueue, intIt->req, -arbitrationDelay);
                            	interferenceManager->addLatency(InterferenceManager::InterconnectDelivery, intIt->req, arbitrationDelay);

                            	interferenceManager->addInterference(InterferenceManager::InterconnectDelivery, intIt->req, arbitrationDelay);
                            }
                        }
                    }
                }
            }
        }
    }

    if(toSlave){

    	for(int i=0;i<cpu_count;i++){
    		if(!sentForCPUID[i] && !(*queue)[i].empty() && !isDeliveryInterference[i]){
    			list<RingRequestEntry>::iterator intIt = (*queue)[i].begin();
    			for( ; intIt != (*queue)[i].end() ; intIt++){
    				intIt->req->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] += arbitrationDelay;
    				if(intIt->req->cmd == Read){
    					interferenceManager->addInterference(InterferenceManager::InterconnectRequestQueue, intIt->req, arbitrationDelay);
    				}
    			}
    		}
    	}
    }
    else{
    	for(int i=0;i<cpu_count;i++){
    		if(!sentForCPUID[i]){
    			for(int j=0;j<queue->size();j++){
    				list<RingRequestEntry>::iterator intIt = (*queue)[j].begin();
					for( ; intIt != (*queue)[j].end() ; intIt++){
						MemReqPtr posDelReq = intIt->req;
						if(posDelReq->interferenceAccurateSenderID == i){
							posDelReq->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT] += arbitrationDelay;
							if(posDelReq->cmd == Read){
								interferenceManager->addInterference(InterferenceManager::InterconnectResponseQueue, posDelReq, arbitrationDelay);
							}
						}
					}
    			}
    		}
    	}
    }
}

void
Ring::updateDeliveryMeasurements(MemReqPtr& req, bool masterToSlave, int queueDelay){

	InterferenceManager::LatencyType transferType = (masterToSlave ? InterferenceManager::InterconnectRequestTransfer : InterferenceManager::InterconnectResponseTransfer);
	InterferenceManager::LatencyType queueType = (masterToSlave ? InterferenceManager::InterconnectRequestQueue : InterferenceManager::InterconnectResponseQueue);

	interferenceManager->addLatency(transferType,
									req,
									inSingleCoreMode() ? req->interconnectTransferDelay : req->ringBaselineTransLat);

	if(!inSingleCoreMode()){
		assert(req->ringBaselineTransLat != -1);
		int transferInterference = req->interconnectTransferDelay - req->ringBaselineTransLat;
		interferenceManager->addInterference(transferType, req, transferInterference);
	}
	else{
		assert(req->ringBaselineTransLat == -1);
	}

	interferenceManager->addLatency(queueType, req, queueDelay);
}

void
Ring::deliver(MemReqPtr& req, Tick cycle, int toID, int fromID){

	int queueDelay = (curTick - req->inserted_into_crossbar) - req->interconnectTransferDelay;
	if(!inSingleCoreMode()){
		req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] += queueDelay + req->ringBaselineTransLat;
	}
	else{
		req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] += (curTick - req->inserted_into_crossbar);
	}
    assert(req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] >= 0);
    assert(req->latencyBreakdown[INTERCONNECT_TRANSFER_LAT] >= req->interferenceBreakdown[INTERCONNECT_TRANSFER_LAT]);

    assert(queueDelay >= 0);

    assert(req->adaptiveMHASenderID != -1);
    perCpuTotalDelay[req->adaptiveMHASenderID] += curTick - req->inserted_into_crossbar;
    perCpuRequests[req->adaptiveMHASenderID]++;

    if(allInterfaces[toID]->isMaster()){

    	if(req->cmd == Read){
    		updateDeliveryMeasurements(req, false, queueDelay);
    	}

        DPRINTF(Crossbar, "Delivering to master %d, req from CPU %d\n", toID, req->interferenceAccurateSenderID);
        allInterfaces[toID]->deliver(req);
    }
    else{

    	if(req->cmd == Read){
    		updateDeliveryMeasurements(req, true, queueDelay);
		}

        int toSlaveID = interconnectIDToL2IDMap[toID];
        if(!blockedInterfaces[toSlaveID] && deliverBuffer[toSlaveID].empty()){
            DPRINTF(Crossbar, "Delivering to slave IC ID %d, slave id %d, req from CPU %d, %d reqs in flight\n", toID, toSlaveID, req->interferenceAccurateSenderID, inFlightRequests[toSlaveID]);

            allInterfaces[toID]->access(req);
            inFlightRequests[toSlaveID]--;
        }
        else{
            assert(curTick >= detailedSimStartTick);
            DPRINTF(Crossbar, "Slave IC ID %d (slave id %d) is blocked, delivery queued req from CPU %d, %d reqs in flight\n", toID, toSlaveID, req->interferenceAccurateSenderID, inFlightRequests[toSlaveID]);

            deliverBuffer[toSlaveID].push_back(RingRequestEntry(req, curTick, vector<int>(), -1));
            assert(deliverBuffer[toSlaveID].size() <= recvBufferSize);
        }
    }
}

void
Ring::clearBlocked(int fromInterface){
    Interconnect::clearBlocked(fromInterface);

    int unblockedSlaveID = interconnectIDToL2IDMap[fromInterface];
    while(!deliverBuffer[unblockedSlaveID].empty()){
        RingRequestEntry entry = deliverBuffer[unblockedSlaveID].front();
        deliverBuffer[unblockedSlaveID].pop_front();

        DPRINTF(Crossbar, "Delivering to slave IC ID %d, slave id %d, req from CPU %d, %d reqs in flight, %d buffered\n", fromInterface, unblockedSlaveID, entry.req->interferenceAccurateSenderID, inFlightRequests[unblockedSlaveID], deliverBuffer[unblockedSlaveID].size());

        entry.req->latencyBreakdown[INTERCONNECT_DELIVERY_LAT] += curTick - entry.enteredAt;
        deliverBufferDelay += curTick - entry.enteredAt;
        deliverBufferRequests++;

        //TODO: assumes all delivery latency is interference
        entry.req->interferenceBreakdown[INTERCONNECT_DELIVERY_LAT] += curTick - entry.enteredAt;

        if(entry.req->cmd == Read){
        	interferenceManager->addLatency(InterferenceManager::InterconnectDelivery, entry.req, curTick - entry.enteredAt);
        	interferenceManager->addInterference(InterferenceManager::InterconnectDelivery, entry.req, curTick - entry.enteredAt);
        }

        MemAccessResult res = allInterfaces[fromInterface]->access(entry.req);
        inFlightRequests[unblockedSlaveID]--;

        if(res == BA_BLOCKED) break;
    }
}

#ifndef DOXYGEN_SHOULD_SKIP_THIS

BEGIN_DECLARE_SIM_OBJECT_PARAMS(Ring)
    Param<int> width;
    Param<int> clock;
    Param<int> transferDelay;
    Param<int> arbitrationDelay;
    Param<int> cpu_count;
    SimObjectParam<HierParams *> hier;
    SimObjectParam<AdaptiveMHA *> adaptive_mha;
    Param<Tick> detailed_sim_start_tick;
    Param<int> single_proc_id;
    SimObjectParam<InterferenceManager* > interference_manager;
    Param<int> fixed_roundtrip_latency;
END_DECLARE_SIM_OBJECT_PARAMS(Ring)

BEGIN_INIT_SIM_OBJECT_PARAMS(Ring)
    INIT_PARAM(width, "the width of the crossbar transmission channels"),
    INIT_PARAM(clock, "crossbar clock"),
    INIT_PARAM(transferDelay, "crossbar transfer delay in CPU cycles"),
    INIT_PARAM(arbitrationDelay, "crossbar arbitration delay in CPU cycles"),
    INIT_PARAM(cpu_count, "the number of CPUs in the system"),
    INIT_PARAM_DFLT(hier, "Hierarchy global variables", &defaultHierParams),
    INIT_PARAM_DFLT(adaptive_mha, "AdaptiveMHA object", NULL),
    INIT_PARAM(detailed_sim_start_tick, "The tick detailed simulation starts"),
    INIT_PARAM_DFLT(single_proc_id, "the expected CPU ID if there is only one processor", -1),
    INIT_PARAM_DFLT(interference_manager, "The interference manager related to this interconnect", NULL),
    INIT_PARAM_DFLT(fixed_roundtrip_latency, "Models infinite bandwidth, fixed latency shared memory system", -1)

END_INIT_SIM_OBJECT_PARAMS(Ring)

CREATE_SIM_OBJECT(Ring)
{
    return new Ring(getInstanceName(),
                              width,
                              clock,
                              transferDelay,
                              arbitrationDelay,
                              cpu_count,
                              hier,
                              adaptive_mha,
                              detailed_sim_start_tick,
                              single_proc_id,
							  interference_manager,
							  fixed_roundtrip_latency);
}

REGISTER_SIM_OBJECT("Ring", Ring)

#endif //DOXYGEN_SHOULD_SKIP_THIS
