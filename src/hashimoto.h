#ifndef HASHIMOTO_H
#define HASHIMOTO_H
#include "dag.h"
#include "uint256.h"
class CBlockHeader;

struct CHashimotoResult {
    uint256 cmix;
    uint256 result;
};

CHashimotoResult hashimoto(CBlockHeader blockToHash);

#endif // HASHIMOTO_H
