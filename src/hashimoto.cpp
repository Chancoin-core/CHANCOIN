#include "hashimoto.h"
#include "main.h"

CHashimotoResult hashimoto(CBlockHeader blockToHash) {
    uint64_t n = floor(get_full_size(0) / HASH_BYTES);
    uint64_t w = floor(MIX_BYTES / WORD_BYTES);
    uint64_t mixhashes = MIX_BYTES / WORD_BYTES;
    std::vector<uint8_t> header;
    uint256 hashedHeader;
    std::copy((uint8_t*)&blockToHash,(uint8_t*)&blockToHash + 80, header.begin());
    lyra2re2_hash((char *)header.data(),(char*)&hashedHeader);
    uint8_t mix[MIX_BYTES];
    for(int i = 0; i < (MIX_BYTES / HASH_BYTES);i++) {
        memcpy(mix + (i * HASH_BYTES), &hashedHeader, HASH_BYTES);
    }
    for(int i = 0; i < ACCESSES; i++) {
        uint64_t p = fnv(i ^ header[0], mix[i % w]) % ((uint64_t)floor(n / mixhashes) * mixhashes);
        uint8_t newdata[MIX_BYTES];
        for(int j = 0; j < MIX_BYTES / HASH_BYTES; j++) {
            CDAGItem item = dag.GetNode(p+j);
            memcpy(newdata + (j * HASH_BYTES), item.node, HASH_BYTES);
        }
        for(int i = 0; i < MIX_BYTES; i++) {
            mix[i] = fnv(mix[i], newdata[i]);
        }
    }
    uint8_t cmix[16];
    for(int i = 0; i < MIX_BYTES; i += 4) {
        cmix[i/4] = fnv(fnv(fnv(mix[i], mix[i+1]), mix[i+2]), mix[i+3]);
    }
    CHashimotoResult result;
    memcpy(&result.cmix, cmix, MIX_BYTES/4);
    std::vector<uint8_t> hash;
    std::copy((uint8_t*)&blockToHash,(uint8_t*)&blockToHash + 80, hash.begin());
    std::copy(cmix, cmix + 80, hash.end());
    lyra2re2_hash96((char *)hash.data(), (char *)&result.result);
    return result;

}
