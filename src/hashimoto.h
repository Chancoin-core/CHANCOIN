#ifndef HASHIMOTO_H
#define HASHIMOTO_H
#include "dag.h"
#include "uint256.h"
class CBlockHeader;

struct CHashimotoResult {
    uint256 cmix;
    uint256 result;
};

extern CHashimotoResult hashimoto(CBlockHeader blockToHash);

extern CHashimotoResult fastimoto(CBlockHeader blockToHash, uint64_t full_size);
#endif // HASHIMOTO_H
