#include "dag.h"
#include "crypto/sph_blake.h"
#include "crypto/Lyra2RE.h"
#include "sync.h"
#include "primitives/block.h"
#include <fstream>

CDAGNode::CDAGNode(uint32_t *ptr, bool fGraphDerived) {
    this->ptr = ptr;
    this->fGraphDerived = fGraphDerived;
}

uint32_t *CDAGNode::GetNodePtr() {
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

std::map<size_t, std::array<uint8_t, 32>> CDAGSystem::seedCache = std::map<size_t, std::array<uint8_t, 32>>();
std::map<size_t, std::vector<uint32_t>> CDAGSystem::cacheCache = std::map<size_t, std::vector<uint32_t>>();
std::map<size_t, std::vector<uint32_t>> CDAGSystem::graphCache = std::map<size_t, std::vector<uint32_t>>();
CHashimotoResult CDAGSystem::lastwork = CHashimotoResult(uint128(), uint256());
void CDAGSystem::PopulateSeedEpoch(uint64_t epoch) {
    if(!(seedCache.find(epoch) == seedCache.end())){
        return;
    }
    static CCriticalSection cs;
    {
        LOCK(cs);
        //Finds largest epoch, populates seed from it.
        seedCache[0].fill(0);
        uint64_t epoch_latest = seedCache.rbegin()->first;
        uint64_t start_epoch = ((epoch - epoch_latest) > epoch) ? 0 : epoch_latest;
        seedCache[epoch] = seedCache[start_epoch];
        for(uint64_t i = start_epoch; i < epoch; i++) {
            //Repeatedly hashes (seed_epoch - start_epoch) times
            sph_blake256_context ctx;
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, seedCache[epoch].data(), HASH_BYTES);
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
    if ((num % 2 == 0) && num > 2)
        return false;

    for(uint64_t i = 3; i < sqrt(num); i += 2) {
        if(num % i == 0)
            return false;
    }
    return true;
}

uint64_t CDAGSystem::GetCacheSize(uint64_t epoch) {
    if(!cacheCache[epoch].empty())
        return cacheCache[epoch].size() * sizeof(uint32_t);
    uint64_t size = CACHE_BYTES_INIT + (CACHE_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= HASH_BYTES;
    while(!is_prime(size / HASH_BYTES)) {
        size -= MIX_BYTES;
    }
    return size;
}

uint64_t CDAGSystem::GetGraphSize(uint64_t epoch) {
    if(!graphCache[epoch].empty())
        return graphCache[epoch].size() * sizeof(uint32_t);
    uint64_t size = DATASET_BYTES_INIT + (DATASET_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= MIX_BYTES;
    while(!is_prime(size / MIX_BYTES)) {
        size -= 2 * MIX_BYTES;
    }
    return size;
}

void CDAGSystem::CreateCacheInPlace(uint64_t epoch) {
    PopulateSeedEpoch(epoch);
    uint64_t size = GetCacheSize(epoch);
    uint64_t items = size / HASH_BYTES;
    cacheCache[epoch].resize(size / sizeof(uint32_t), 0);
    sph_blake256_context ctx;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, seedCache[epoch].data(), HASH_BYTES);
    sph_blake256_close(&ctx, cacheCache[epoch].data());
    //First 32 bytes of cache are written to with hash of seed.
    for(uint64_t i = 1; i < items; i++){
        //Hash last item of the cache repeatedly the generate all items.
        uint8_t hasheditem[HASH_BYTES];
        sph_blake256_init(&ctx);
        sph_blake256(&ctx, cacheCache[epoch].data() + (i - 1)*(HASH_BYTES / sizeof(uint32_t)), HASH_BYTES);
        sph_blake256_close(&ctx, hasheditem);
        std::memcpy(cacheCache[epoch].data() + i*(HASH_BYTES / sizeof(uint32_t)), hasheditem, HASH_BYTES);
    }
    for(uint64_t round = 0; round < CACHE_ROUNDS; round++) {
        //3 round randmemohash.
        for(uint64_t i = 0; i < items; i++) {
            uint64_t target = cacheCache[epoch][(i * (HASH_BYTES / sizeof(uint32_t)))] % items;
            uint64_t mapper = (i - 1 + items) % items;
            /* Map target onto mapper, hash it,
             * then replace the current cache item with the 32 byte result. */
            uint32_t item[HASH_BYTES / sizeof(uint32_t)];
            for(uint64_t dword = 0; dword < (HASH_BYTES / sizeof(uint32_t)); dword++) {
                item[dword] = cacheCache[epoch][(mapper * (HASH_BYTES / sizeof(uint32_t))) + dword]
                            ^ cacheCache[epoch][(target * (HASH_BYTES / sizeof(uint32_t))) + dword];
            }
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, item, HASH_BYTES);
            sph_blake256_close(&ctx, item);
            std::memcpy(cacheCache[epoch].data() + (i * (HASH_BYTES / sizeof(uint32_t))), item, HASH_BYTES);
        }
    }

}

void CDAGSystem::CreateGraphInPlace(uint64_t epoch) {
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    graphCache[epoch].resize(items * (HASH_BYTES / WORD_BYTES), 0);
    for(uint64_t i = 0; i < items; i++) {
        CDAGNode node = GetNode(i, epoch*EPOCH_LENGTH);
        memcpy(graphCache[epoch].data() + (i * (HASH_BYTES / sizeof(uint32_t))), node.GetNodePtr(), HASH_BYTES);
    }
}

void CDAGSystem::PopulateCacheEpoch(uint64_t epoch) {
    if(!cacheCache[epoch].empty()) {
        return;
    }
    static CCriticalSection cs;
    {
        LOCK(cs);
        if(epoch > 1) {
            if(cacheCache[epoch - 2].size() > 0) {
                cacheCache[epoch - 2] = std::vector<uint32_t>();
            }
        }
        PopulateSeedEpoch(epoch);
        CreateCacheInPlace(epoch);
    }
}

void CDAGSystem::PopulateGraphEpoch(uint64_t epoch) {
    if(!graphCache[epoch].empty()) {
        return;
    }
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
    uint64_t epoch = height / EPOCH_LENGTH;
    PopulateCacheEpoch(epoch);
    uint64_t items = GetCacheSize(epoch) / HASH_BYTES;
    uint32_t *mix = new uint32_t[HASH_BYTES / sizeof(uint32_t)];
    std::memcpy(mix, cacheCache[epoch].data() + ((i % items) * (HASH_BYTES / sizeof(uint32_t))), HASH_BYTES);
    mix[0] ^= i;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    for(uint64_t parent = 0; parent < DATASET_PARENTS; parent++) {
        uint64_t index = fnv(i ^ parent, mix[parent % (HASH_BYTES / sizeof(uint32_t))]) % items;
        for(uint64_t dword = 0; dword < (HASH_BYTES / sizeof(uint32_t)); dword++) {
            mix[dword] = fnv(mix[dword], cacheCache[epoch][index * (HASH_BYTES / sizeof(uint32_t))]);
        }
    }
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    return CDAGNode(mix, false);
}

CDAGNode CDAGSystem::GetNodeFromGraph(uint64_t i, int32_t height) {
    uint64_t epoch = height / EPOCH_LENGTH;
    PopulateGraphEpoch(epoch);
    return CDAGNode(graphCache[epoch].data() + (i * (HASH_BYTES / sizeof(uint32_t))), true);
}

CHashimotoResult CDAGSystem::Hashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    const uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint32_t headerhash[HASH_BYTES / sizeof(uint32_t)];
    uint32_t mix[MIX_BYTES / sizeof(uint32_t)];
    uint32_t cmix[(MIX_BYTES / sizeof(uint32_t)) / sizeof(uint32_t)];
    lyra2re2_hash((char*)&header, (char*)headerhash);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::memcpy(mix + (i * (HASH_BYTES / sizeof(uint32_t))), headerhash, HASH_BYTES);
    }
    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = (fnv(i ^ headerhash[0], mix[i % (MIX_BYTES / sizeof(uint32_t))]) % (items / mixhashes)) * mixhashes;
        uint32_t mapdata[MIX_BYTES / sizeof(uint32_t)];
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNode(target + mixhash, header.height);
            assert((mixhash * (HASH_BYTES / sizeof(uint32_t))) < 16);
            std::memcpy(mapdata + (mixhash * (HASH_BYTES / sizeof(uint32_t))), node.GetNodePtr(), HASH_BYTES);
        }
        for(uint64_t dword = 0; dword < (MIX_BYTES / sizeof(uint32_t)); dword++) {
            mix[dword] = fnv(mix[dword], mapdata[dword]);
            assert(dword < sizeof(mix));
        }
    }
    for(uint64_t i = 0; i < MIX_BYTES / sizeof(uint32_t); i += sizeof(uint32_t)) {
        cmix[i / sizeof(uint32_t)] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
        assert((i/4) < sizeof(cmix));
    }
    uint128 resmix;
    uint256 result;
    std::memcpy(resmix.begin(), cmix, MIX_BYTES / sizeof(uint32_t));
    std::array<uint8_t, HEADER_BYTES> fhash;
    std::memcpy(fhash.data(), headerhash, HASH_BYTES);
    std::memcpy(fhash.data() + HASH_BYTES, &header.height, sizeof(int32_t));
    std::memcpy(fhash.data() + HASH_BYTES + sizeof(int32_t), cmix, MIX_BYTES / sizeof(uint32_t));
    lyra2re2_hash52((char*)fhash.data(), (char*)result.begin());
    return CHashimotoResult(resmix, result);
}

CHashimotoResult CDAGSystem::FastHashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    const uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint32_t headerhash[HASH_BYTES / sizeof(uint32_t)];
    uint32_t mix[MIX_BYTES / sizeof(uint32_t)];
    uint32_t cmix[(MIX_BYTES / sizeof(uint32_t)) / sizeof(uint32_t)];
    lyra2re2_hash((char*)&header, (char*)headerhash);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::memcpy(mix + (i * (HASH_BYTES / sizeof(uint32_t))), headerhash, HASH_BYTES);
    }
    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = fnv(i ^ headerhash[0], mix[i % (MIX_BYTES / sizeof(uint32_t))]) % (items / mixhashes) * mixhashes;
        uint32_t mapdata[MIX_BYTES / sizeof(uint32_t)];
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNodeFromGraph(target + mixhash, header.height);
            std::memcpy(mapdata + (mixhash * (HASH_BYTES / sizeof(uint32_t))), node.GetNodePtr(), HASH_BYTES);
        }
        for(uint64_t dword = 0; dword < (MIX_BYTES / sizeof(uint32_t)); dword++) {
            mix[dword] = fnv(mix[dword], mapdata[dword]);
        }
    }
    for(uint64_t i = 0; i < MIX_BYTES / sizeof(uint32_t); i += sizeof(uint32_t)) {
        cmix[i / sizeof(uint32_t)] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 resmix;
    uint256 result;
    std::memcpy(resmix.begin(), cmix, MIX_BYTES / sizeof(uint32_t));
    std::array<uint8_t, HEADER_BYTES> fhash;
    std::memcpy(fhash.data(), headerhash, HASH_BYTES);
    std::memcpy(fhash.data() + HASH_BYTES, &header.height, sizeof(int32_t));
    std::memcpy(fhash.data() + HASH_BYTES + sizeof(int32_t), cmix, MIX_BYTES / sizeof(uint32_t));
    lyra2re2_hash52((char*)fhash.data(), (char*)result.begin());
    return CHashimotoResult(resmix, result);
}
