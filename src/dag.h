#ifndef DAG_H
#define DAG_H
#include <math.h>
#include <cstdlib>
#include <string.h>
#include <map>
#include "sync.h"
#include "Lyra2RE/Lyra2RE.h"
#include "Lyra2RE/sph_keccak.h"
#include "Lyra2RE/sph_blake.h"
#define WORD_BYTES 4
#define DATASET_BYTES_INIT 536870912
#define DATASET_BYTES_GROWTH 12582912
#define CACHE_BYTES_INIT 8388608
#define CACHE_BYTES_GROWTH 196608
#define EPOCH_LENGTH 400
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
    unsigned long sz = CACHE_BYTES_INIT + (CACHE_BYTES_GROWTH * round(sqrt(6*floor(((float)block_number / (float)EPOCH_LENGTH)))));
    sz -= HASH_BYTES;
    while (!is_prime(sz / HASH_BYTES)) {
        sz -= 2 * HASH_BYTES;
    }
    return sz;
}

inline unsigned long get_full_size(unsigned long block_number) {
    unsigned long sz = DATASET_BYTES_INIT + (DATASET_BYTES_GROWTH * round(sqrt(6*floor(((float)block_number / (float)EPOCH_LENGTH)))));
    sz -= MIX_BYTES;
    while (!is_prime(sz / MIX_BYTES)) {
        sz -= 2 * MIX_BYTES;
    }
    return sz;
}

extern char *calc_dataset_item(char *cache, unsigned long i, unsigned long cache_size);

extern char* calc_full_dataset(char *cache, unsigned long dataset_size);

extern char* mkcache(unsigned long size, char* seed);
class CDAGItem {
public:
    char *node;
    CDAGItem (unsigned long i,char *cache, unsigned long cache_size) {
        node = calc_dataset_item(cache, i, cache_size);
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
    std::map<size_t, char[32]> seed;
    std::map<size_t, char *> cache;
    std::map<size_t, char *> fdag;
public:
    CDAGSystem() {
        memset(seed[0], 0, 32);
    }

    CDAGItem GetNode(unsigned long i, unsigned long height) {
        sph_blake256_context ctx_blake;
        static CCriticalSection cs;
        {
            LOCK(cs);
            if(cache[(unsigned long)floor(height/200.0)] == nullptr) {
                if (seed.find((unsigned long)floor(height/200.0)) == seed.end()) {
                    memset(seed[(unsigned long)floor(height/200.0)], 0, 32);
                    for(unsigned long iters = 0; iters < (unsigned long)floor(height/200.0); iters++) {
                        sph_blake256_init(&ctx_blake);
                        sph_blake256 (&ctx_blake,seed[(unsigned long)floor(height/200.0)], 32);
                        sph_blake256_close(&ctx_blake, seed[(unsigned long)floor(height/200.0)]);
                    }
                }
                cache[(unsigned long)floor(height/200.0)] = mkcache(get_cache_size(height), seed[(unsigned long)floor(height/200.0)]);
            }
        }
        return CDAGItem(i, cache[(unsigned long)floor(height/200.0)], get_cache_size(height));
    }

    CDAGFullDerivItem GetFullNodeDeriv(unsigned long i, unsigned long height) {
        sph_blake256_context ctx_blake;
        static CCriticalSection cs;
        {
            LOCK(cs);
            if(cache[(unsigned long)floor(height/200.0)] == nullptr) {
                if (seed.find((unsigned long)floor(height/200.0)) == seed.end()) {
                    memset(seed[(unsigned long)floor(height/200.0)], 0, 32);
                    for(unsigned long iters = 0; iters < (unsigned long)floor(height/200.0); iters++) {
                        sph_blake256_init(&ctx_blake);
                        sph_blake256 (&ctx_blake,seed[(unsigned long)floor(height/200.0)], 32);
                        sph_blake256_close(&ctx_blake, seed[(unsigned long)floor(height/200.0)]);
                    }
                }
                cache[(unsigned long)floor(height/200.0)] = mkcache(get_cache_size(height), seed[(unsigned long)floor(height/200.0)]);
            }
            if (fdag.find((unsigned long)floor(height/200.0)) == fdag.end()) {
                fdag[(unsigned long)floor(height/200.0)] = calc_full_dataset(cache[(unsigned long)floor(height/200.0)], get_full_size(height));
            }
        }
        return CDAGFullDerivItem(i, fdag[(unsigned long)floor(height/200.0)]);
    }
};

#endif // DAG_H
