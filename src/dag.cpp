#include "dag.h"
#include "crypto/sph_blake.h"

CDAGNode::CDAGNode(uint8_t *ptr, bool fGraphDerived) {
    this->ptr = ptr;
    this->fGraphDerived = fGraphDerived;
}

CDAGNode::GetNodePtr() {
    return ptr;
}

CDAGNode::~CDAGNode() {
    if(!fGraphDerived) {
        delete[] ptr;
    }
}

void CDAGSystem::PopulateSeedEpoch(uint64_t epoch) {
    if(!(this->seedCache.find(epoch) == this->seedCache.end())){
        return;
    }
    static CCriticalSection cs;
    {
        LOCK(cs);
        //Finds largest epoch, populates seed from it.
        uint64_t epoch_latest = this->seedCache.rbegin()->first;
        uint64_t start_epoch = ((epoch - epoch_latest) > epoch) ? 0 : epoch_latest;
        seedCache[epoch] = seedCache[start_epoch];
        for(uint64_t i = start_epoch; i < epoch; i++) {
            //Repeatedly hashes (seed_epoch - start_epoch) times
            sph_blake256_context ctx;
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, seedCache[epoch], 32);
            sph_blake256_close(&ctx,seedCache[epoch]);
        }
    }
}

void CDAGSystem::CreateGraphInPlace(uint64_t epoch, uint64_t size, std::array<uint8_t, 32> seed) {
    cacheCache[epoch].resize(size, 0);
    uint64_t items =

}

void CDAGSystem::PopulateCacheEpoch(uint64_t epoch) {
    if(!this->cacheCache[epoch].empty()) {
        return;
    }
    PopulateSeedEpoch(epoch);
}
