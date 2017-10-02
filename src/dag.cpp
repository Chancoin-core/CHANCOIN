#include "dag.h"

char* mkcache(unsigned long size, char *seed){
    unsigned long n = floor(size / HASH_BYTES);
    sph_blake256_context ctx_blake;
    char *cache = (char*)malloc(size);
    sph_blake256_init(&ctx_blake);
    sph_blake256 (&ctx_blake,seed, 32);
    sph_blake256_close(&ctx_blake, cache);
    for(int i = 0; i < (n - 1); i++) {
        sph_blake256_init(&ctx_blake);
        sph_blake256 (&ctx_blake,cache + ((i)*32), 32);
        sph_blake256_close(&ctx_blake, (cache + ((i+1)*32)));
    }
    for(int z = 0; z < 3; z++) {
        for(int i = 0; i < n; i++) {
            unsigned char mapped[32];
            unsigned long v = cache[(i*32)] % n;
            for(int k = 0; k < 32; k++) {
                mapped[k] = cache[(((i-1+n) % n)*32)+k] ^ cache[(v*32)+k];
            }
            sph_blake256_init(&ctx_blake);
            sph_blake256 (&ctx_blake,mapped, 32);
            sph_blake256_close(&ctx_blake, mapped);
            memcpy(cache + (i*32), mapped, 32);
        }
    }
    return cache;
}

char *calc_dataset_item(char *cache, unsigned long i, unsigned long cache_size) {
   unsigned long r = floor(HASH_BYTES / (float)WORD_BYTES);
   sph_blake256_context ctx_blake;
   /*if(i > r) {
       gpulog(LOG_INFO, 0, "%u", (i*32 % cache_size));
   }*/
   char *mix = (char*)malloc(32);
   memcpy(mix, cache + ((i)*32 % cache_size), 32);
   (mix)[0] ^= (i);
   sph_blake256_init(&ctx_blake);
   sph_blake256 (&ctx_blake,mix, 32);
   sph_blake256_close(&ctx_blake, mix);
   for(int j = 0; j < DATASET_PARENTS; j++) {
       unsigned long cache_index = fnv((i) ^ j, (mix)[j % r])*32;
       for(int k = 0; k < 32; k++) {
           mix[k] = fnv(mix[k], cache[(cache_index % cache_size)+k]);
       }
   }
   sph_blake256_init(&ctx_blake);
   sph_blake256 (&ctx_blake,mix, 32);
   sph_blake256_close(&ctx_blake, mix);
   return mix;
}

char* calc_full_dataset(char *cache, unsigned long dataset_size, unsigned long cache_size) {
    char *fullset = new char[dataset_size];
    for(int i = 0; i < (floor(dataset_size / HASH_BYTES)); i++){
        char *item = calc_dataset_item(cache, i, cache_size);
        memcpy(fullset + i*32, item, 32);
        free(item);
    }
    return fullset;
}
