#ifndef DAG_H
#define DAG_H

#include <map>
#include <vector>
#include "uint256.h"

class CBlockHeader;

class CDAGNode {
private:
    uint8_t *ptr;
    bool fGraphDerived;
public:
    CDAGNode(uint8_t *ptr, bool fGraphDerived);

    uint8_t* GetNodePtr();

    ~CDAGNode();

};

class CHashimotoResult {
private:
    uint128 cmix;
    uint256 result;
public:
    CHashimotoResult(uint128 cmix, uint256 result);

    uint128 GetCmix();

    uint256 GetResult();
};

class CDAGSystem {
private:
    /** Constants */
    static const uint32_t WORD_BYTES = 4;
    static const uint32_t DATASET_BYTES_INIT = 536870912;
    static const uint32_t DATASET_BYTES_GROWTH = 12582912;
    static const uint32_t CACHE_BYTES_INIT = 8388608;
    static const uint32_t CACHE_BYTES_GROWTH = 196608;
    static const uint32_t EPOCH_LENGTH = 400;
    static const uint32_t CACHE_MULTIPLIER = 64;
    static const uint32_t HEADER_BYTES = 56;
    static const uint32_t MIX_BYTES = 64;
    static const uint32_t HASH_BYTES = 32;
    static const uint32_t DATASET_PARENTS = 256;
    static const uint32_t CACHE_ROUNDS = 3;
    static const uint32_t ACCESSES = 64;
    static const uint32_t FNV_PRIME = 0x01000193;

    /** Trivial primality check */
    static bool is_prime(uint64_t num);

    /** FNV hash function */
    static uint32_t fnv(uint32_t v1, uint32_t v2);

    /** Get cache size from epoch */
    static uint64_t GetCacheSize(uint64_t epoch);

    /** Get graph size from epoch */
    static uint64_t GetGraphSize(uint64_t epoch);

    /** Caches cache seeds in case of a need for regeneration */
    static std::map<size_t, std::array<uint8_t, 32>> seedCache;
    /** Caches caches to verify blocks */
    static std::map<size_t, std::vector<uint8_t>> cacheCache;
    /** Caches dags to generate blocks faster */
    static std::map<size_t, std::vector<uint8_t>> graphCache;

    /** Populates seed of the epoch's epoch. */
    static void PopulateSeedEpoch(uint64_t epoch);
    /** Populates cache of the epoch's epoch.*/
    static void PopulateCacheEpoch(uint64_t epoch);
    /** Populates graph of the epoch's epoch */
    static void PopulateGraphEpoch(uint64_t epoch);

    /** Actually creates a cache from seed and size data */
    static void CreateCacheInPlace(uint64_t epoch);

    /** Actually creates a graph from a cache */
    static void CreateGraphInPlace(uint64_t epoch);

public:
    /** Gets node from cache, much slower(on the order of 100x, but uses about that much less memory. */
    static CDAGNode GetNode(uint64_t i, int32_t height);
    /**
     * Gets node from cached graph, delayed to generate the needed graph(which may take several minutes), but once it
     * is generated, returning a node is effectively instant.
     */
    static CDAGNode GetNodeFromGraph(uint64_t i, int32_t height);

    /** Runs the hashimoto function on header using the cache */
    static CHashimotoResult Hashimoto(CBlockHeader header);

    /** Runs the hashimoto function on header using the graph */
    static CHashimotoResult FastHashimoto(CBlockHeader header);
};

#endif // DAG_H
