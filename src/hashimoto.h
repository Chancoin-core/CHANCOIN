#ifndef HASHIMOTO_H
#define HASHIMOTO_H
#include "dag.h"
#include "uint256.h"
class CBlockHeader;

struct CHashimotoResult {
    uint256 cmix;
    uint256 result;
};

template <class CDAGItemType>
CHashimotoResult hashimoto(CBlockHeader blockToHash);

#include "hashimototemplatehack.h"
#endif // HASHIMOTO_H
