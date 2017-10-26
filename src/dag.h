#ifndef DAG_H
#define DAG_H

#include "sync.h"

#include <map>

class CDAGNode {
private:
    uint8_t *ptr;
    bool fGraphDerived;
public:
    CDAGNode(uint8_t *ptr, bool fGraphDerived);

    uint8_t* GetNodePtr();

    ~CDAGNode();

};

class CDAGSystem {
private:
    /** Constants */
    const uint32_t DATASET_BYTES_INIT = 536870912;
    const uint32_t DATASET_BYTES_GROWTH = 12582912;
    const uint32_t CACHE_BYTES_INIT = 8388608;
    const uint32_t CACHE_BYTES_GROWTH = 196608;
    const uint32_t EPOCH_LENGTH = 400;
    const uint32_t CACHE_MULTIPLIER = 64;
    const uint32_t MIX_BYTES = 64;
    const uint32_t HASH_BYTES = 32;
    const uint32_t DATASET_PARENTS = 256;
    const uint32_t CACHE_ROUNDS = 3;
    const uint32_t ACCESSES = 64;
    const uint32_t FNV_PRIME = 0x01000193;

    /** Trivial primality check */
    bool is_prime(uint64_t num);

    /** FNV hash function */
    uint32_t fnv(uint32_t v1, uint32_t v2);

    /** Get cache size from epoch */
    uint64_t GetCacheSize(uint64_t epoch);

    /** Get graph size from epoch */
    uint64_t GetGraphSize(uint64_t epoch);

    /** Get graph node from cache */
    CDAGNode DeriveGraphNode(uint64_t epoch, uint64_t i);

    /** Caches cache seeds in case of a need for regeneration */
    std::map<size_t, std::array<uint8_t, 32>> seedCache;
    /** Caches caches to verify blocks */
    std::map<size_t, std::vector<uint8_t>> cacheCache;
    /** Caches dags to generate blocks faster */
    std::map<size_t, std::vector<uint8_t>> graphCache;

    /** Populates seed of the epoch's epoch. */
    void PopulateSeedEpoch(uint64_t epoch);
    /** Populates cache of the epoch's epoch.*/
    void PopulateCacheEpoch(uint64_t epoch);
    /** Populates graph of the epoch's epoch */
    void PopulateGraphEpoch(uint64_t epoch);

    /** Actually creates a graph from seed and size data */
    void CreateCacheInPlace(uint64_t epoch);

    void CreateGraphInPlace(uint64_t epoch);

public:
    /** Gets node from cache, much slower(on the order of 100x, but uses about that much less memory. */
    CDAGNode GetNode(uint64_t i, int32_t height);
    /**
     * Gets node from cached graph, delayed to generate the needed graph(which may take several minutes), but once it
     * is generated, returning a node is effectively instant.
     */
    CDAGNode GetNodeFromGraph(uint64_t i, int32_t height);

};

#endif // DAG_H
