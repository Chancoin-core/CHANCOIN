#ifndef HASHIMOTO_H
#define HASHIMOTO_H
#include "dag.h"
#include "uint256.h"
#include "main.h"
struct CHashimotoResult {
    uint256 cmix;
    uint256 result;
};

extern CHashimotoResult hashimoto(CBlockHeader blockToHash);

#endif // HASHIMOTO_H
