#include "dag.h"
#include "crypto/sph_blake.h"
#include "crypto/Lyra2RE.h"
#include "sync.h"
#include "primitives/block.h"

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
        return cacheCache[epoch].size();
    uint64_t size = CACHE_BYTES_INIT + (CACHE_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= HASH_BYTES;
    while(!is_prime(size / HASH_BYTES)) {
        size -= MIX_BYTES;
    }
    return size;
}

uint64_t CDAGSystem::GetGraphSize(uint64_t epoch) {
    if(!graphCache[epoch].empty())
        return graphCache[epoch].size();
    uint64_t size = DATASET_BYTES_INIT + (DATASET_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= MIX_BYTES;
    while(!is_prime(size / MIX_BYTES)) {
        size -= 2 * MIX_BYTES;
    }
    return size;
}

void CDAGSystem::CreateCacheInPlace(uint64_t epoch) {
    uint64_t size = GetCacheSize(epoch);
    cacheCache[epoch].resize(size / WORD_BYTES, 0);
    uint64_t items = size / HASH_BYTES;
    uint64_t hashwords = HASH_BYTES / WORD_BYTES;
    sph_blake256_context ctx;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, seedCache[epoch].data(), HASH_BYTES);
    sph_blake256_close(&ctx, cacheCache[epoch].data());
    for(uint64_t i = 0; i < (items - 1); i++) {
        sph_blake256_init(&ctx);
        sph_blake256(&ctx, cacheCache[epoch].data() + (i * (hashwords)), HASH_BYTES);
        sph_blake256_close(&ctx, cacheCache[epoch].data() + (i+1)*hashwords);
    }

    for(uint32_t rounds = 0; rounds < CACHE_ROUNDS; rounds++) {
        for(uint32_t i = 0; i < items; i++) {
            uint32_t item[hashwords];
            uint64_t current = ((i - 1 + items) % items);
            uint64_t target = cacheCache[epoch][i*hashwords] % items;
            for(uint64_t byte = 0; byte < hashwords; byte++) {
                item[byte] = cacheCache[epoch][(current * hashwords) + byte] ^ cacheCache[epoch][(target * hashwords) + byte];
            }
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, item, HASH_BYTES);
            sph_blake256_close(&ctx, cacheCache[epoch].data() + (i * hashwords));
        }
    }
}

void CDAGSystem::CreateGraphInPlace(uint64_t epoch) {
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    graphCache[epoch].resize(items * (HASH_BYTES / WORD_BYTES), 0);
    for(uint64_t i = 0; i < items; i++) {
        CDAGNode node = GetNode(i, epoch*EPOCH_LENGTH);
        std::copy((uint8_t*)node.GetNodePtr(), (uint8_t*)node.GetNodePtr() + HASH_BYTES, (uint8_t*)graphCache[epoch].data() + (i * (HASH_BYTES)));
    }
}

void CDAGSystem::PopulateCacheEpoch(uint64_t epoch) {
    if(!cacheCache[epoch].empty()) {
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
    uint64_t hashwords = HASH_BYTES / WORD_BYTES;
    uint32_t *mix = new uint32_t[hashwords];
    std::memcpy(mix, cacheCache[epoch].data() + (i % items)*(HASH_BYTES), 32);
    //std::copy((uint8_t*)(cacheCache[epoch].data()) + (i % items)*(HASH_BYTES), (uint8_t*)(cacheCache[epoch].data()) + (((i % items)*HASH_BYTES)+HASH_BYTES), mix);
    mix[0] ^= i;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    for(uint64_t item = 0; item < DATASET_PARENTS; item++) {
        uint64_t index = fnv(i ^ item, mix[item % hashwords]);
        for(uint64_t byte = 0; byte < hashwords; byte++) {
            mix[byte] = fnv(mix[byte], cacheCache[epoch][((index % items)*hashwords) + byte]);
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
    CDAGNode node(graphCache[epoch].data() + (i * (HASH_BYTES / WORD_BYTES)), true);
    return node;
}

CHashimotoResult CDAGSystem::Hashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    uint64_t wordhashes = MIX_BYTES / WORD_BYTES;
    uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint64_t hashwords = HASH_BYTES / WORD_BYTES;
    uint32_t hashedheader[hashwords];
    uint32_t mix[wordhashes];
    uint32_t cmix[wordhashes / WORD_BYTES];
    lyra2re2_hash((char*)&header.nVersion, (char*)hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy((uint8_t*)hashedheader, (uint8_t*)hashedheader + HASH_BYTES, (uint8_t*)mix + (i * HASH_BYTES));
    }

    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = fnv(i ^ hashedheader[0], mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNode(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < hashwords; byte++) {
                mix[(mixhash * hashwords) + byte] = fnv(mix[(mixhash * hashwords) + byte], node.GetNodePtr()[byte]);
            }
        }
    }

    for(uint64_t i = 0; i < wordhashes; i += 4) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy((uint8_t*)cmix, (uint8_t*)cmix + (MIX_BYTES / WORD_BYTES), cmix_res.begin());
    std::array<uint8_t, HEADER_BYTES> finalhash;
    std::copy((uint8_t*)hashedheader, (uint8_t*)hashedheader + HASH_BYTES, finalhash.begin());
    std::copy(((uint8_t*)&header.height), ((uint8_t*)&header.height) + WORD_BYTES, finalhash.begin() + HASH_BYTES);
    std::copy((uint8_t*)cmix, (uint8_t*)cmix + (MIX_BYTES / WORD_BYTES), finalhash.begin() + HASH_BYTES + WORD_BYTES);
    lyra2re2_hash52((char*)finalhash.data(), (char*)result.begin());
    return lastwork = CHashimotoResult(cmix_res, result);
}

CHashimotoResult CDAGSystem::FastHashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    uint64_t wordhashes = MIX_BYTES / WORD_BYTES;
    uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint64_t hashwords = HASH_BYTES / WORD_BYTES;
    uint32_t hashedheader[hashwords];
    uint32_t mix[wordhashes];
    uint32_t cmix[wordhashes / WORD_BYTES];
    lyra2re2_hash((char*)&header.nVersion, (char*)hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy((uint8_t*)hashedheader, (uint8_t*)hashedheader + HASH_BYTES, (uint8_t*)mix + (i * HASH_BYTES));
    }

    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = fnv(i ^ hashedheader[0], mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNodeFromGraph(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < hashwords; byte++) {
                mix[(mixhash * hashwords) + byte] = fnv(mix[(mixhash * hashwords) + byte], node.GetNodePtr()[byte]);
            }
        }
    }

    for(uint64_t i = 0; i < wordhashes; i += 4) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy((uint8_t*)cmix, (uint8_t*)cmix + (MIX_BYTES / WORD_BYTES), cmix_res.begin());
    std::array<uint8_t, HEADER_BYTES> finalhash;
    std::copy((uint8_t*)hashedheader, (uint8_t*)hashedheader + HASH_BYTES, finalhash.begin());
    std::copy(((uint8_t*)&header.height), ((uint8_t*)&header.height) + WORD_BYTES, finalhash.begin() + HASH_BYTES);
    std::copy((uint8_t*)cmix, (uint8_t*)cmix + (MIX_BYTES / WORD_BYTES), finalhash.begin() + HASH_BYTES + WORD_BYTES);
    lyra2re2_hash52((char*)finalhash.data(), (char*)result.begin());
    return lastwork = CHashimotoResult(cmix_res, result);
}
