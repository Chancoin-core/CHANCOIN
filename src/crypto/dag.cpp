#include "dag.h"
#include "crypto/sph_blake.h"
#include "crypto/Lyra2RE.h"
#include "sync.h"
#include "primitives/block.h"

CDAGNode::CDAGNode(uint8_t *ptr, bool fGraphDerived) {
    this->ptr = ptr;
    this->fGraphDerived = fGraphDerived;
}

uint8_t *CDAGNode::GetNodePtr() {
    return ptr;
}

CDAGNode::~CDAGNode() {
    if(!fGraphDerived) {
        delete[] ptr;
    }
}

CHashimotoResult::CHashimotoResult(uint128 cmix, uint256 result) {
    cmix = cmix;
    result = result;
}

uint128 CHashimotoResult::GetCmix() {
    return cmix;
}

uint256 CHashimotoResult::GetResult() {
    return result;
}

std::map<size_t, std::array<uint8_t, 32>> CDAGSystem::seedCache = std::map<size_t, std::array<uint8_t, 32>>();
std::map<size_t, std::vector<uint8_t>> CDAGSystem::cacheCache = std::map<size_t, std::vector<uint8_t>>();
std::map<size_t, std::vector<uint8_t>> CDAGSystem::graphCache = std::map<size_t, std::vector<uint8_t>>();

void CDAGSystem::PopulateSeedEpoch(uint64_t epoch) {
    if(!(seedCache.find(epoch) == seedCache.end())){
        return;
    }
    static CCriticalSection cs;
    {
        LOCK(cs);
        //Finds largest epoch, populates seed from it.
        seedCache[0];
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
    uint64_t size = CACHE_BYTES_INIT + (CACHE_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= HASH_BYTES;
    while(!is_prime(size / HASH_BYTES)) {
        size -= MIX_BYTES;
    }
    return size;
}

uint64_t CDAGSystem::GetGraphSize(uint64_t epoch) {
    uint64_t size = DATASET_BYTES_INIT + (DATASET_BYTES_GROWTH * round(sqrt(6*epoch)));
    size -= MIX_BYTES;
    while(!is_prime(size / MIX_BYTES)) {
        size -= 2 * MIX_BYTES;
    }
    return size;
}

void CDAGSystem::CreateCacheInPlace(uint64_t epoch) {
    uint64_t size = GetCacheSize(epoch);
    cacheCache[epoch].resize(size, 0);
    uint64_t items = size / HASH_BYTES;
    sph_blake256_context ctx;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, seedCache[epoch].data(), HASH_BYTES);
    sph_blake256_close(&ctx, cacheCache[epoch].data());
    for(uint64_t i = 0; i < (items - 1); i++) {
        sph_blake256_init(&ctx);
        sph_blake256(&ctx, cacheCache[epoch].data() + i * HASH_BYTES, HASH_BYTES);
        sph_blake256_close(&ctx, cacheCache[epoch].data() + (i+1)*HASH_BYTES);
    }

    for(uint32_t rounds = 0; rounds < CACHE_ROUNDS; rounds++) {
        for(uint32_t i = 0; i < items; i++) {
            uint8_t item[HASH_BYTES];
            uint64_t current = ((i - 1 + items) % items);
            uint64_t target = cacheCache[epoch][i*HASH_BYTES] % items;
            for(uint64_t byte = 0; byte < HASH_BYTES; byte++) {
                item[byte] = cacheCache[epoch][(current * HASH_BYTES) + byte] ^ cacheCache[epoch][(target * HASH_BYTES) + byte];
            }
            sph_blake256_init(&ctx);
            sph_blake256(&ctx, item, HASH_BYTES);
            sph_blake256_close(&ctx, cacheCache[epoch].data() + (i * HASH_BYTES));
        }
    }
}

void CDAGSystem::CreateGraphInPlace(uint64_t epoch) {
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    graphCache[epoch].resize(items * HASH_BYTES, 0);
    for(uint64_t i = 0; i < items; i++) {
        CDAGNode node = GetNode(i, epoch*EPOCH_LENGTH);
        std::copy(node.GetNodePtr(), node.GetNodePtr() + HASH_BYTES, graphCache[epoch].data() + (i * HASH_BYTES));
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
    uint8_t *mix = new uint8_t[HASH_BYTES];
    std::copy(cacheCache[epoch].begin() + (i % items)*HASH_BYTES, cacheCache[epoch].begin() + (((i % items)*HASH_BYTES)+HASH_BYTES), mix);
    mix[0] ^= i;
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    for(uint64_t item = 0; item < DATASET_PARENTS; item++) {
        uint64_t index = fnv(i ^ item, mix[item % hashwords]);
        for(uint64_t byte = 0; byte < HASH_BYTES; byte++) {
            mix[byte] = fnv(mix[byte], cacheCache[epoch][(index % items) + byte]);
        }
    }
    sph_blake256_init(&ctx);
    sph_blake256(&ctx, mix, HASH_BYTES);
    sph_blake256_close(&ctx, mix);
    std::cout << (uint64_t)mix << std::endl;
    return CDAGNode(mix, false);
}

CDAGNode CDAGSystem::GetNodeFromGraph(uint64_t i, int32_t height) {
    uint64_t epoch = height / EPOCH_LENGTH;
    PopulateGraphEpoch(epoch);
    CDAGNode node(graphCache[epoch].data() + (i*HASH_BYTES), true);
    return node;
}

CHashimotoResult CDAGSystem::Hashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    uint64_t wordhashes = MIX_BYTES / WORD_BYTES;
    uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint8_t hashedheader[HASH_BYTES];
    uint8_t mix[MIX_BYTES];
    uint8_t cmix[MIX_BYTES / WORD_BYTES];
    lyra2re2_hash((char*)&header, (char*)hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy(hashedheader, hashedheader + HASH_BYTES, mix + (i * HASH_BYTES));
    }

    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = fnv(i ^ *(uint32_t*)hashedheader, mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNode(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < HASH_BYTES; byte++) {
                std::cout << (uint64_t)node.GetNodePtr() << std::endl;
                *node.GetNodePtr() += 1;
                std::cout << mixhash << std::endl;
                std::cout << byte << std::endl;
                mix[(mixhash * HASH_BYTES) + byte] = fnv(mix[(mixhash * HASH_BYTES) + byte], *(node.GetNodePtr() + (mixhash * HASH_BYTES) + byte));
            }
        }
    }

    for(uint64_t i = 0; i < MIX_BYTES; i += 4) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy(cmix, cmix + (MIX_BYTES / 4), cmix_res.begin());
    std::array<uint8_t, HEADER_BYTES> finalhash;
    std::copy(hashedheader, hashedheader + HASH_BYTES, finalhash.begin());
    std::copy(&header.height, &header.height + sizeof(int32_t), finalhash.begin() + HASH_BYTES);
    std::copy(cmix, cmix + (MIX_BYTES / 4), finalhash.begin() + HASH_BYTES + sizeof(int32_t));
    lyra2re2_hash56((char*)finalhash.data(), (char*)result.begin());
    return CHashimotoResult(cmix_res, result);
}

CHashimotoResult CDAGSystem::FastHashimoto(CBlockHeader header) {
    uint64_t epoch = header.height / EPOCH_LENGTH;
    uint64_t items = GetGraphSize(epoch) / HASH_BYTES;
    uint64_t wordhashes = MIX_BYTES / WORD_BYTES;
    uint64_t mixhashes = MIX_BYTES / HASH_BYTES;
    uint8_t hashedheader[HASH_BYTES];
    uint8_t mix[MIX_BYTES];
    uint8_t cmix[MIX_BYTES / WORD_BYTES];
    lyra2re2_hash((char*)&header, (char*)hashedheader);
    for(uint64_t i = 0; i < mixhashes; i++) {
        std::copy(hashedheader, hashedheader + HASH_BYTES, mix + (i * HASH_BYTES));
    }

    for(uint64_t i = 0; i < ACCESSES; i++) {
        uint32_t target = fnv(i ^ *(uint32_t*)hashedheader, mix[i % wordhashes]) % (items / mixhashes) * mixhashes;
        for(uint64_t mixhash = 0; mixhash < mixhashes; mixhash++) {
            CDAGNode node = GetNodeFromGraph(target + mixhash, header.height);
            for(uint64_t byte = 0; byte < HASH_BYTES; byte++) {
                mix[(mixhash * HASH_BYTES) + byte] = fnv(mix[(mixhash * HASH_BYTES) + byte], *(node.GetNodePtr() + (mixhash * HASH_BYTES) + byte));
            }
        }
    }

    for(uint64_t i = 0; i < MIX_BYTES; i += 4) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    uint128 cmix_res;
    uint256 result;
    std::copy(cmix, cmix + (MIX_BYTES / 4), cmix_res.begin());
    std::array<uint8_t, HEADER_BYTES> finalhash;
    std::copy(hashedheader, hashedheader + HASH_BYTES, finalhash.begin());
    std::copy(&header.height, &header.height + sizeof(int32_t), finalhash.begin() + HASH_BYTES);
    std::copy(cmix, cmix + (MIX_BYTES / 4), finalhash.begin() + HASH_BYTES + sizeof(int32_t));
    lyra2re2_hash56((char*)finalhash.data(), (char*)result.begin());
    return CHashimotoResult(cmix_res, result);
}
