#ifndef DAG_H
#define DAG_H
#include <math.h>
#include <cstdlib>
#include <string.h>
#include "Lyra2RE/Lyra2RE.h"
#include "Lyra2RE/sph_keccak.h"
#include "Lyra2RE/sph_blake.h"
#define WORD_BYTES 4
#define DATASET_BYTES_INIT 536870912
#define DATASET_BYTES_GROWTH 4194304
#define CACHE_BYTES_INIT 8388608
#define CACHE_BYTES_GROWTH 65536
#define EPOCH_LENGTH 60000
#define CACHE_MULTIPLIER 1024
#define MIX_BYTES 64
#define HASH_BYTES 32
#define DATASET_PARENTS 256
#define CACHE_ROUNDS 3
#define ACCESSES 64
#define FNV_PRIME 0x01000193

inline int is_prime(unsigned long number) {
    if (number <= 1) return false;
    if((number % 2 == 0) && number > 2) return false;
    for(unsigned long i = 3; i < number / 2; i += 2) {
        if(number % i == 0)
            return true;
    }
    return true;
}

inline unsigned int fnv(unsigned int v1, unsigned int v2) {
    return ((v1 * FNV_PRIME)  ^ v2) % (0xffffffff);
}

inline unsigned long get_cache_size(unsigned long block_number) {
    unsigned long sz = CACHE_BYTES_INIT + (CACHE_BYTES_GROWTH * floor(((float)block_number / (float)EPOCH_LENGTH)));
    sz -= HASH_BYTES;
    while (!is_prime(sz / HASH_BYTES)) {
        sz -= 2 * HASH_BYTES;
    }
    return sz;
}

inline unsigned long get_full_size(unsigned long block_number) {
    unsigned long sz = DATASET_BYTES_INIT + (DATASET_BYTES_GROWTH * floor((float)block_number / (float)EPOCH_LENGTH));
    sz -= MIX_BYTES;
    while (!is_prime(sz / MIX_BYTES)) {
        sz -= 2 * MIX_BYTES;
    }
    return sz;
}

extern char *calc_dataset_item(char *cache, unsigned long i);

extern char* calc_full_dataset(char *cache);

extern char* mkcache(unsigned long size, char* seed);
class CDAGItem {
public:
    char *node;
    CDAGItem (unsigned long i,char *cache) {
        node = calc_dataset_item(cache, i);
    }

    ~CDAGItem() {
        free(node);
    }
};

class CDAGFullDerivItem {
public:
    char *node;
    CDAGFullDerivItem (unsigned long i, char *fdag) {
        node = fdag + (i*32);
    }

    ~CDAGFullDerivItem() {

    }
};


class CDAGSystem {
private:
    char seed[32];
    char *cache;
    char *fdag = NULL;
public:
    CDAGSystem() {
        memset(seed, 0, 32);
        cache = mkcache(get_cache_size(0), seed);
        fdag = calc_full_dataset(cache);
    }

    CDAGItem GetNode(unsigned long i) {
        return CDAGItem(i, cache);
    }

    CDAGFullDerivItem GetFullNodeDerv(unsigned long i) {
        if(!fdag) {
            //fdag = calc_full_dataset(cache);
        }
        return CDAGFullDerivItem(i, fdag);
    }
};

#endif // DAG_H
