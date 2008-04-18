/**
 * @file
 * Describes the Memory Controller API.
 */

#include <sstream>

#include "mem/bus/controller/rdfcfs_memory_controller.hh"

using namespace std;

RDFCFSTimingMemoryController::RDFCFSTimingMemoryController() { 
  num_active_pages = 0;
  max_active_pages = 4;

  close = new MemReq();
  close->cmd = Close;
  activate = new MemReq();
  activate->cmd = Activate;

  lastIsWrite = false;

}

/** Frees locally allocated memory. */
RDFCFSTimingMemoryController::~RDFCFSTimingMemoryController(){
}

int RDFCFSTimingMemoryController::insertRequest(MemReqPtr &req) {

    req->inserted_into_memory_controller = curTick;
    if (req->cmd == Read) {
        readQueue.push_back(req);
        if (readQueue.size() > readqueue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    if (req->cmd == Writeback) {
        writeQueue.push_back(req);
        if (writeQueue.size() > writequeue_size) { // full queue + one in progress
            setBlocked();
        }
    }
    
    assert(req->cmd == Read || req->cmd == Writeback);
  
    // Early activation of reads
    if ((req->cmd == Read) && (num_active_pages < max_active_pages)) {
        if (!isActive(req) && bankIsClosed(req)) {
            // Activate that page
            activate->paddr = req->paddr;
            activate->flags &= ~SATISFIED;
            activePages.push_back(getPage(req));
            num_active_pages++;
            assert(mem_interface->calculateLatency(activate) == 0);
        }
    }
    
    return 0;
}

bool RDFCFSTimingMemoryController::hasMoreRequests() {
    
    if(lastIssuedReq){
        if(lastIsWrite){
            writeQueue.remove(lastIssuedReq);
        }
        else{
            readQueue.remove(lastIssuedReq);
        }
    }
    
    if (readQueue.empty() && writeQueue.empty() && (num_active_pages == 0)) {
      return false;
    } else {
      return true;
    }
}

MemReqPtr& RDFCFSTimingMemoryController::getRequest() {

    if (num_active_pages < max_active_pages) { 
        
        // Go through all lists to see if we can activate anything
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                activePages.push_back(getPage(tmp));
                num_active_pages++;
                
                return (activate);
            }
        } 
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (!isActive(tmp) && bankIsClosed(tmp)) {
                //Request is not active and bank is closed. Activate it
                activate->paddr = tmp->paddr;
                activate->flags &= ~SATISFIED;
                activePages.push_back(getPage(tmp));
                num_active_pages++;
                
                return (activate);
            }
        }        
    }

    // Check if we can close the first page (eg, there is no active requests to this page
    for (pageIterator = activePages.begin(); pageIterator != activePages.end(); pageIterator++) {
        Addr Active = *pageIterator;
        bool canClose = true;
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (getPage(tmp) == Active) {
                canClose = false; 
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (getPage(tmp) == Active) {
                canClose = false;
            }
        }

        if (canClose) {
            close->paddr = getPageAddr(Active);
            assert(Active << 10 == getPageAddr(Active));
            close->flags &= ~SATISFIED;
            activePages.erase(pageIterator);
            num_active_pages--;
            return close;
        }
    }
  
    // Go through the active pages and find a ready operation
    for (pageIterator = activePages.begin(); pageIterator != activePages.end() ; pageIterator++) {
        for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {

                if(isBlocked() && 
                    readQueue.size() <= readqueue_size && 
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
        
                lastIssuedReq = tmp;
                lastIsWrite = false;
                
                return tmp;
            }
        }
        for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
            MemReqPtr& tmp = *queueIterator;
            if (isReady(tmp)) {

                if(isBlocked() &&
                    readQueue.size() <= readqueue_size && 
                    writeQueue.size() <= writequeue_size){
                    setUnBlocked();
                }
        
                lastIssuedReq = tmp;
                lastIsWrite = true;
                return tmp;
            }
        }
    }
  
    // No ready operation, issue any active operation 
    for (queueIterator = readQueue.begin(); queueIterator != readQueue.end(); queueIterator++) {
        MemReqPtr& tmp = *queueIterator;
        if (isActive(tmp)) {

            if(isBlocked() && 
                readQueue.size() <= readqueue_size && 
                writeQueue.size() <= writequeue_size){
                setUnBlocked();
            }
        
            lastIssuedReq = tmp;
            lastIsWrite = false;
            return tmp;
        }
    }
    for (queueIterator = writeQueue.begin(); queueIterator != writeQueue.end(); queueIterator++) {
        MemReqPtr& tmp = *queueIterator;
        if (isActive(tmp)) {

            if(isBlocked() &&
                readQueue.size() <= readqueue_size && 
                writeQueue.size() <= writequeue_size){
                setUnBlocked();
            }
        
            lastIssuedReq = tmp;
            lastIsWrite = true;
            return tmp;
        }
    }

    fatal("This should never happen!");
    return close;
}
