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
            sph_blake256(&ctx, seedCache[epoch].data(), 32);
            sph_blake256_close(&ctx,seedCache[epoch].data());
        }
    }
}

uint32_t CDAGSystem::fnv(uint32_t v1, uint32_t v2) {
    return ((v1 * FNV_PRIME) ^ v2) % UINT32_MAX;
}

bool CDAGSystem::is_prime(uint64_t num) {
    if (num < 2)
        return false;
    if ((num % 2 == 0) && number > 2)
        return false;

    for(uint64_t i = 3; i < sqrt(num); i += 2) {
        if(num % i == 0)
            return false;
    }
    return true;
}

uint64_t CDAGSystem::GetCacheSize(uint64_t epoch) {
    uint64_t size = this->CACHE_BYTES_INIT + (this->CACHE_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= HASH_BYTES;
    while(!is_prime(size)) {
        size -= MIX_BYTES;
    }
    return size;
}

uint64_t CDAGSystem::GetGraphSize(uint64_t epoch) {
    return GetCacheSize(epoch) * this->CACHE_MULTIPLIER;
}

void CDAGSystem::CreateCacheInPlace(uint64_t epoch) {
    uint64_t size = GetCacheSize(epoch);
    this->cacheCache[epoch].resize(size, 0);
    uint64_t items = size / this->HASH_BYTES;
    sph_blake256_context ctx;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, this->seedCache[epoch].data(), this->HASH_BYTES);
    sph_blake256_close(&ctx, this->cacheCache[epoch].data());
    for(uint64_t i = 0; i < (items - 1); i++) {
        sph_blake256_init(&ctx);
        sph_blake256(&ctx, this->cacheCache[epoch].data() + i * this->HASH_BYTES, this->HASH_BYTES);
        sph_blake256_close(&ctx, this->cacheCache[epoch].data() + (i+1)*this->HASH_BYTES);
    }

    for(uint32_t rounds = 0; rounds < this->CACHE_ROUNDS; rounds++) {
        for(uint32_t i = 0; i < items; i++) {
            uint8_t item[this->HASH_BYTES];
            uint64_t current = ((i - 1 + items) % items);
            uint64_t target = this->cacheCache[i*this->HASH_BYTES] % items;
            for(uint64_t byte = 0; byte < this->HASH_BYTES; byte++) {
                item[byte] = this->cacheCache[(current * this->HASH_BYTES) + byte] ^ this->cacheCache[(target * this->HASH_BYTES) + byte];
            }
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, item, 32);
            sph_blake256_close(&ctx, this->cacheCache[i * this->HASH_BYTES]);
        }
    }
}

void CDAGSystem::PopulateCacheEpoch(uint64_t epoch) {
    if(!this->cacheCache[epoch].empty()) {
        return;
    }
    static CCriticalSection cs;
    {
        LOCK(cs);
        PopulateSeedEpoch(epoch);
        CreateCacheInPlace(epoch);
    }
}
