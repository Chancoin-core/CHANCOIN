// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "pow.h"

#include "arith_uint256.h"
#include "chain.h"
#include "primitives/block.h"
#include "uint256.h"
#include "util.h"
#include "crypto/dag.h"

unsigned int KimotoGravityWell(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {

        unsigned int npowWorkLimit = UintToArith256(params.powLimit).GetCompact();
	int blockstogoback = 0;

	//set default to pre-v2.0 values
	int64_t retargetTimespan = params.nPowTargetTimespanV2;
	int64_t retargetInterval = params.nPowTargetSpacing;

	// Genesis block
	if (pindexLast == NULL)
		return npowWorkLimit;

	// DigiByte: This fixes an issue where a 51% attack can change difficulty at will.
	// Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
	blockstogoback = retargetInterval-1;
	if ((pindexLast->nHeight+1) != retargetInterval)
		blockstogoback = retargetInterval;

	// Go back by what we want to be 14 days worth of blocks
	const CBlockIndex* pindexFirst = pindexLast;
	for (int i = 0; pindexFirst && i < blockstogoback; i++)
		pindexFirst = pindexFirst->pprev;
	assert(pindexFirst);

	// Limit adjustment step
	int64_t nActualTimespan = pindexLast->GetBlockTime() - pindexFirst->GetBlockTime();
	LogPrintf("nActualTimespan = %d  before bounds\n", nActualTimespan);

	// thanks to RealSolid & WDC for this code
	LogPrintf("GetNextWorkRequired nActualTimespan Limiting\n");
	if (nActualTimespan < (retargetTimespan - (retargetTimespan/4)) ) nActualTimespan = (retargetTimespan - (retargetTimespan/4));
	if (nActualTimespan > (retargetTimespan + (retargetTimespan/2)) ) nActualTimespan = (retargetTimespan + (retargetTimespan/2));

	arith_uint256 bnNew;
	arith_uint256 bnBefore;
	bnNew.SetCompact(pindexLast->nBits);
	bnBefore=bnNew;
	bnNew *= nActualTimespan;
	bnNew /= retargetTimespan;

	if (bnNew > UintToArith256(params.powLimit))
		bnNew = UintToArith256(params.powLimit);

	// debug print
	LogPrintf("GetNextWorkRequired RETARGET\n");
	LogPrintf("nTargetTimespan = %d    nActualTimespan = %d\n", retargetTimespan, nActualTimespan);
	LogPrintf("Before: %08x  %s\n", pindexLast->nBits, ArithToUint256(bnBefore).ToString());
	LogPrintf("After:  %08x  %s\n", bnNew.GetCompact(), ArithToUint256(bnNew).ToString());

	return bnNew.GetCompact();
}

unsigned int GetNextWorkRequired_legacy(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params)
{
    assert(pindexLast != nullptr);
    unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();

    // Only change once per difficulty adjustment interval
    if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0)
    {
        if (params.fPowAllowMinDifficultyBlocks)
        {
            // Special difficulty rule for testnet:
            // If the new block's timestamp is more than 2* 10 minutes
            // then allow mining of a min-difficulty block.
            if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                return nProofOfWorkLimit;
            else
            {
                // Return the last non-special-min-difficulty-rules-block
                const CBlockIndex* pindex = pindexLast;
                while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                    pindex = pindex->pprev;
                return pindex->nBits;
            }
        }
        return pindexLast->nBits;
    }

    // Go back by what we want to be 14 days worth of blocks
    // Chancoin: This fixes an issue where a 51% attack can change difficulty at will.
    // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
    int blockstogoback = params.DifficultyAdjustmentInterval()-1;
    if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval())
        blockstogoback = params.DifficultyAdjustmentInterval();

    // Go back by what we want to be 14 days worth of blocks
    const CBlockIndex* pindexFirst = pindexLast;
    for (int i = 0; pindexFirst && i < blockstogoback; i++)
        pindexFirst = pindexFirst->pprev;

    assert(pindexFirst);

    return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
}

unsigned int CalculateNextWorkRequired(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params& params)
{
    if (params.fPowNoRetargeting)
        return pindexLast->nBits;

    // Limit adjustment step
    int64_t nActualTimespan = pindexLast->GetBlockTime() - nFirstBlockTime;
    if (nActualTimespan < params.nPowTargetTimespanV1/4)
        nActualTimespan = params.nPowTargetTimespanV1/4;
    if (nActualTimespan > params.nPowTargetTimespanV1*4)
        nActualTimespan = params.nPowTargetTimespanV1*4;

    // Retarget
    arith_uint256 bnNew;
    arith_uint256 bnOld;
    bnNew.SetCompact(pindexLast->nBits);
    bnOld = bnNew;
    // Chancoin: intermediate uint256 can overflow by 1 bit
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    bool fShift = bnNew.bits() > bnPowLimit.bits() - 1;
    if (fShift)
        bnNew >>= 1;
    bnNew *= nActualTimespan;
    bnNew /= params.nPowTargetTimespanV1;
    if (fShift)
        bnNew <<= 1;

    if (bnNew > bnPowLimit)
        bnNew = bnPowLimit;

    return bnNew.GetCompact();
}

bool CheckProofOfWork(CBlockHeader header, const Consensus::Params& params, bool fFast, bool fNoCheckHashMix)
{
    bool fNegative;
    bool fOverflow;
    arith_uint256 bnTarget;
    CDAGSystem sys;
    bnTarget.SetCompact(header.nBits, &fNegative, &fOverflow);

    // Check range
    if (fNegative || bnTarget == 0 || fOverflow || bnTarget > UintToArith256(params.powLimit))
        return false;

    if (header.nVersion & 0x00000100) {
        if (!fFast) {
            CHashimotoResult res = sys.Hashimoto(header);
            if (header.hashMix != res.GetCmix() && !fNoCheckHashMix)
                return false;
        } else {
            CHashimotoResult res = sys.FastHashimoto(header);
            if (header.hashMix != res.GetCmix() && !fNoCheckHashMix)
                return false;
            if (UintToArith256(res.GetResult()) > bnTarget)
                return false;
            else
                return true;
        }
    }

    // Check proof of work matches claimed amount
    if (UintToArith256(header.GetPoWHash()) > bnTarget)
        return false;

    return true;
}

unsigned int GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {

    int RetargetMode = 1;
    volatile bool debug = false;
    if(debug) {
        assert(pindexLast != nullptr);
        unsigned int nProofOfWorkLimit = UintToArith256(params.powLimit).GetCompact();
        if ((pindexLast->nHeight+1) % params.DifficultyAdjustmentInterval() != 0) {
               if (params.fPowAllowMinDifficultyBlocks)
               {
                   // Special difficulty rule for testnet:
                   // If the new block's timestamp is more than 2* 10 minutes
                   // then allow mining of a min-difficulty block.
                   if (pblock->GetBlockTime() > pindexLast->GetBlockTime() + params.nPowTargetSpacing*2)
                       return nProofOfWorkLimit;
                   else
                   {
                       // Return the last non-special-min-difficulty-rules-block
                       const CBlockIndex* pindex = pindexLast;
                       while (pindex->pprev && pindex->nHeight % params.DifficultyAdjustmentInterval() != 0 && pindex->nBits == nProofOfWorkLimit)
                           pindex = pindex->pprev;
                       return pindex->nBits;
                   }
               }
               return pindexLast->nBits;
           }

           // Go back by what we want to be 14 days worth of blocks
           // Chancoin: This fixes an issue where a 51% attack can change difficulty at will.
           // Go back the full period unless it's the first retarget after genesis. Code courtesy of Art Forz
           int blockstogoback = params.DifficultyAdjustmentInterval()-1;
           if ((pindexLast->nHeight+1) != params.DifficultyAdjustmentInterval())
               blockstogoback = params.DifficultyAdjustmentInterval();

           // Go back by what we want to be 14 days worth of blocks
           const CBlockIndex* pindexFirst = pindexLast;
           for (int i = 0; pindexFirst && i < blockstogoback; i++)
               pindexFirst = pindexFirst->pprev;

           assert(pindexFirst);

       return CalculateNextWorkRequired(pindexLast, pindexFirst->GetBlockTime(), params);
    }

    if (pindexLast->nHeight+1 >= params.RetargetAlgorithmSwitch) { RetargetMode = 2; }
    if (RetargetMode == 1) { return GetNextWorkRequired_legacy(pindexLast, pblock, params); }
    if (RetargetMode == 2) { return KimotoGravityWell(pindexLast, pblock, params); }

    // if we're here, something weird garnon
    return GetNextWorkRequired_legacy(pindexLast, pblock, params);
}

