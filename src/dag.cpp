#include "dag.h"

char* mkcache(unsigned long size, char *seed){
    unsigned long n = floor(size / HASH_BYTES);
    char *cache = (char*)malloc(size);
    lyra2re2_hash(cache, seed);
    sph_blake256_context ctx_keccak;
    for(int i = 0; i < (n - 1); i++) {
        sph_blake256_init(&ctx_keccak);
        sph_blake256 (&ctx_keccak,cache + ((i)*32), 32);
        sph_blake256_close(&ctx_keccak, (cache + ((i+1)*32)));
    }
    for(int z = 0; z < 3; z++) {
        for(int i = 0; i < n; i++) {
            unsigned char mapped[32];
            unsigned long v = cache[(i*32)] % n;
            for(int k = 0; k < 32; k++) {
                mapped[k] = cache[(((i-1+n) % n)*32)+k] ^ cache[(v*32)+k];
            }
            lyra2re2_hash((char*)mapped, (char*)mapped);
            memcpy(cache + (i*32), mapped, 32);
        }
    }
    return cache;
}

char *calc_dataset_item(char *cache, unsigned long i) {
   unsigned long cache_size = get_cache_size(0);
   unsigned long r = floor(HASH_BYTES / (float)WORD_BYTES);
   sph_blake256_context ctx_keccak;
   /*if(i > r) {
       gpulog(LOG_INFO, 0, "%u", (i*32 % cache_size));
   }*/
   char *mix = (char*)malloc(32);
   memcpy(mix, cache + ((i)*32 % cache_size), 32);
   (mix)[0] ^= (i);
   sph_blake256_init(&ctx_keccak);
   sph_blake256 (&ctx_keccak,mix, 32);
   sph_blake256_close(&ctx_keccak, mix);
   for(int j = 0; j < DATASET_PARENTS; j++) {
       unsigned long cache_index = fnv((i) ^ j, (mix)[j % r])*32;
       for(int k = 0; k < 32; k++) {
           mix[k] = fnv(mix[k], cache[(cache_index % cache_size)+k]);
       }
   }
   sph_blake256_init(&ctx_keccak);
   sph_blake256 (&ctx_keccak,mix, 32);
   sph_blake256_close(&ctx_keccak, mix);
   return mix;
}

char* calc_full_dataset(char *cache) {
    char *fullset = new char[get_full_size(0)];
    for(int i = 0; i < (floor(get_full_size(0) / HASH_BYTES)); i++){
        char *item = calc_dataset_item(cache, i);
        memcpy(fullset + i*32, item, 32);
        free(item);
    }
    return fullset;
}
