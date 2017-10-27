#include "dag.h"
#include "crypto/sph_blake.h"
#include "crypto/Lyra2RE.h"
#include "sync.h"
#include "uint256.h"
#include "primitives/block.h"

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

CHashimotoResult::CHashimotoResult(uint128 cmix, uint256 result) {
    this->cmix = cmix;
    this->result = result;
}

uint128 CHashimotoResult::GetCmix() {
    return cmix;
}

uint256 CHashimotoResult::GetResult() {
    return result;
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
            sph_blake256(&ctx, seedCache[epoch].data(), this->HASH_BYTES);
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
            uint64_t target = this->cacheCache[epoch][i*this->HASH_BYTES] % items;
            for(uint64_t byte = 0; byte < this->HASH_BYTES; byte++) {
                item[byte] = this->cacheCache[epoch][(current * this->HASH_BYTES) + byte] ^ this->cacheCache[epoch][(target * this->HASH_BYTES) + byte];
            }
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, item, this->HASH_BYTES);
            sph_blake256_close(&ctx, this->cacheCache[epoch][i * this->HASH_BYTES]);
        }
    }
}

void CDAGSystem::CreateGraphInPlace(uint64_t epoch) {
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    this->graphCache[epoch].resize(items * HASH_BYTES, 0);
    for(uint64_t i = 0; i < items; i++) {
        CDAGNode node = GetNode(i, epoch*this->EPOCH_LENGTH);
        std::copy(node.GetNodePtr(), node.GetNodePtr() + this->HASH_BYTES, this->graphCache[epoch].data() + (i * this->HASH_BYTES));
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

void CDAGSystem::PopulateGraphEpoch(uint64_t epoch) {
    static CCriticalSection cs;
    {
        LOCK(cs);
        PopulateSeedEpoch(epoch);
        PopulateCacheEpoch(epoch);
        CreateGraphInPlace(epoch);
    }
}

CDAGNode CDAGSystem::GetNode(uint64_t i, int32_t height) {
    sph_blake256_context ctx;
    uint64_t epoch = height / this->EPOCH_LENGTH;
    PopulateCacheEpoch(epoch);
    uint64_t items = GetCacheSize(epoch) / this->HASH_BYTES;
    uint64_t hashwords = this->HASH_BYTES / this->WORD_BYTES;
    uint8_t *mix = new uint8_t[32];
    std::copy(this->cacheCache[epoch].begin() + (i % items)*this->HASH_BYTES, this->cacheCache[epoch].begin() + (((i % items)*this->HASH_BYTES)+this->HASH_BYTES), mix);
    mix[0] ^= i;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, this->HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    for(uint64_t item = 0; item < DATASET_PARENTS; item++) {
        uint64_t index = fnv(i ^ item, mix[item % hashwords]);
        for(uint64_t byte = 0; byte < this->HASH_BYTES; byte++) {
            mix[byte] = fnv(mix[byte], this->cacheCache[epoch][(index % items) + byte]);
        }
    }
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, this->HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    CDAGNode node(mix, false);
    return node;
}

CDAGNode CDAGSystem::GetNodeFromGraph(uint64_t i, int32_t height) {
    uint64_t epoch = height / this->EPOCH_LENGTH;
    PopulateGraphEpoch(epoch);
    CDAGNode node(this->graphCache[epoch].data() + (i*this->HASH_BYTES), true);
    return node;
}

CBlockHeader CDAGSystem::Hashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / this->EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / this->HASH_BYTES;
    uint64_t wordhashes = this->MIX_BYTES / this->WORD_BYTES;
    uint64_t mixhashes = this->MIX_BYTES / this->HASH_BYTES;
    uint8_t hashedheader[this->HASH_BYTES];
    uint8_t mix[this->MIX_BYTES];
    uint8_t cmix[this->MIX_BYTES / this->WORD_BYTES];
    lyra2re2_hash(&header,&hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy(hashedheader, hashedheader + this->HASH_BYTES, mix + (i * this->HASH_BYTES));
    }

    for(uint64_t i = 0; i < this->ACCESSES; i++) {
        uint32_t target = fnv(i ^ *(unsigned int*)hashedheader, mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNode(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < this->HASH_BYTES; byte++) {
                mix[(mixhash * this->HASH_BYTES) + byte] = fnv(mix[(mixhash * this->HASH_BYTES) + byte], node.GetNodePtr()[(mixhash * this->HASH_BYTES) + byte]);
            }
        }
    }

    for(uint64_t i = 0; i < this->MIX_BYTES; i++) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy(cmix, cmix + (this->MIX_BYTES / 4), cmix_res.begin());
    std::array<uint8_t, this->HEADER_BYTES> finalhash;
    std::copy(hashedheader, hashedheader + this->HASH_BYTES, finalhash.begin());
    std::copy(&header.height, &header.height + sizeof(int32_t), finalhash.begin() + this->HASH_BYTES);
    std::copy(cmix, cmix + (this->MIX_BYTES / 4), finalhash.begin() + this->HASH_BYTES + sizeof(int32_t));
    lyra2re2_hash104(finalhash.data(), result.begin());
    return CHashimotoResult(cmix_res, result);
}

CBlockHeader CDAGSystem::FastHashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / this->EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / this->HASH_BYTES;
    uint64_t wordhashes = this->MIX_BYTES / this->WORD_BYTES;
    uint64_t mixhashes = this->MIX_BYTES / this->HASH_BYTES;
    uint8_t hashedheader[this->HASH_BYTES];
    uint8_t mix[this->MIX_BYTES];
    uint8_t cmix[this->MIX_BYTES / this->WORD_BYTES];
    lyra2re2_hash(&header,&hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy(hashedheader, hashedheader + this->HASH_BYTES, mix + (i * this->HASH_BYTES));
    }

    for(uint64_t i = 0; i < this->ACCESSES; i++) {
        uint32_t target = fnv(i ^ *(unsigned int*)hashedheader, mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNodeFromGraph(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < this->HASH_BYTES; byte++) {
                mix[(mixhash * this->HASH_BYTES) + byte] = fnv(mix[(mixhash * this->HASH_BYTES) + byte], node.GetNodePtr()[(mixhash * this->HASH_BYTES) + byte]);
            }
        }
    }

    for(uint64_t i = 0; i < this->MIX_BYTES; i++) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy(cmix, cmix + (this->MIX_BYTES / 4), cmix_res.begin());
    std::array<uint8_t, this->HEADER_BYTES> finalhash;
    std::copy(hashedheader, hashedheader + this->HASH_BYTES, finalhash.begin());
    std::copy(&header.height, &header.height + sizeof(int32_t), finalhash.begin() + this->HASH_BYTES);
    std::copy(cmix, cmix + (this->MIX_BYTES / 4), finalhash.begin() + this->HASH_BYTES + sizeof(int32_t));
    lyra2re2_hash104(finalhash.data(), result.begin());
    return CHashimotoResult(cmix_res, result);
}
