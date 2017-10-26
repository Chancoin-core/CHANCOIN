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

    /** Actually creates a graph from seed and size data*/
    void CreateGraphInPlace(uint64_t epoch, uint64_t size, std::array<uint8_t, 32> seed);

public:
    /** Gets node from cache, much slower(on the order of 100x, but uses about that much less memory. */
    CDAGNode GetNode(uint64_t i, int height);
    /**
     * Gets node from cached graph, delayed to generate the needed graph(which may take several minutes), but once it
     * is generated, returning a node is effectively instant.
     */
    CDAGNode GetNodeFromGraph(uint64_t i, int height);

};

#endif // DAG_H
