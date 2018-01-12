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

unsigned int DUAL_KGW3(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params) {

    const CBlockIndex *BlockLastSolved = pindexLast;
    const CBlockIndex *BlockReading = pindexLast;
    uint64_t PastBlocksMass = 0;
    int64_t PastRateActualSeconds = 0;
    int64_t PastRateTargetSeconds = 0;
    double PastRateAdjustmentRatio = double(1);
    arith_uint256 PastDifficultyAverage;
    arith_uint256 PastDifficultyAveragePrev;
    double EventHorizonDeviation;
    double EventHorizonDeviationFast;
    double EventHorizonDeviationSlow;
	
    //DUAL_KGW3 SETUP
    const uint64_t Blocktime = params.nPowTargetSpacing;
    const unsigned int timeDaySeconds = 86400;
    uint64_t pastSecondsMin = timeDaySeconds * 0.025;
    uint64_t pastSecondsMax = timeDaySeconds * 7;
    uint64_t PastBlocksMin = pastSecondsMin / Blocktime;
    uint64_t PastBlocksMax = pastSecondsMax / Blocktime;
    const arith_uint256 bnPowLimit = UintToArith256(params.powLimit);
    if (BlockLastSolved == NULL || BlockLastSolved->nHeight == 0 || 
        (uint64_t)BlockLastSolved->nHeight < PastBlocksMin) {
        return bnPowLimit.GetCompact(); 
    }
    for (unsigned int i = 1; BlockReading && BlockReading->nHeight > 0; i++) {
        if (PastBlocksMax > 0 && i > PastBlocksMax) { break; }
        PastBlocksMass++;
        PastDifficultyAverage.SetCompact(BlockReading->nBits);
        if (i > 1) {
            if(PastDifficultyAverage >= PastDifficultyAveragePrev)
                PastDifficultyAverage = ((PastDifficultyAverage - PastDifficultyAveragePrev) / i) + PastDifficultyAveragePrev;
            else
                PastDifficultyAverage = PastDifficultyAveragePrev - ((PastDifficultyAveragePrev - PastDifficultyAverage) / i);
        }
        PastDifficultyAveragePrev = PastDifficultyAverage;
        PastRateActualSeconds = BlockLastSolved->GetBlockTime() - BlockReading->GetBlockTime();
        PastRateTargetSeconds = Blocktime * PastBlocksMass;
        PastRateAdjustmentRatio = double(1);
        if (PastRateActualSeconds < 0) { PastRateActualSeconds = 0; }
        if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
            PastRateAdjustmentRatio = double(PastRateTargetSeconds) / double(PastRateActualSeconds);
        }
        EventHorizonDeviation = 1 + (0.7084 * pow((double(PastBlocksMass)/double(72)), -1.228));  //28.2 and 144 possible
        EventHorizonDeviationFast = EventHorizonDeviation;
        EventHorizonDeviationSlow = 1 / EventHorizonDeviation;

        if (PastBlocksMass >= PastBlocksMin) {
                if ((PastRateAdjustmentRatio <= EventHorizonDeviationSlow) || (PastRateAdjustmentRatio >= EventHorizonDeviationFast))
                { assert(BlockReading); break; }
        }
        if (BlockReading->pprev == NULL) { assert(BlockReading); break; }
        BlockReading = BlockReading->pprev;
    }
	
    //KGW Original
    arith_uint256 kgw_dual1(PastDifficultyAverage);
    arith_uint256 kgw_dual2;
    kgw_dual2.SetCompact(pindexLast->nBits);
    if (PastRateActualSeconds != 0 && PastRateTargetSeconds != 0) {
         kgw_dual1 *= PastRateActualSeconds;
         kgw_dual1 /= PastRateTargetSeconds;
    }
    uint64_t nActualTime1 = (uint64_t)( pindexLast->GetBlockTime() - pindexLast->pprev->GetBlockTime() );
    uint64_t nActualTimespanshort = (uint64_t) nActualTime1;	
    //if(nActualTime1 < 0) { nActualTime1 = Blocktime; }
    if (nActualTime1 < Blocktime / 3)
        nActualTime1 = Blocktime / 3;
    if (nActualTime1 > Blocktime * 3)
        nActualTime1 = Blocktime * 3;
    kgw_dual2 *= nActualTime1;
    kgw_dual2 /= Blocktime;
	
    //Fusion from Retarget and Classic KGW3 (BitSend=)	
    arith_uint256 bnNew;
    bnNew = ((kgw_dual2 + kgw_dual1)/2);
    //if(nActualTimespanshort < 0) { nActualTimespanshort = 0; }
    LogPrintf("nActualTimespanshort = %d \n", nActualTimespanshort );
    if(nActualTimespanshort < Blocktime/6){
        LogPrintf("ORIG  : %08x %s DIFF \n", bnNew.GetCompact(), bnNew.ToString().c_str());
	const int nLongShortNew1 = 85;
        const int nLongShortNew2 = 100;
        bnNew = bnNew * nLongShortNew1;	
        bnNew = bnNew / nLongShortNew2;
        LogPrintf("ADJUST: %08x %s DIFF \n", bnNew.GetCompact(), bnNew.ToString().c_str() );
    }

    //BitBreak BitSend
    const int nLongTimeLimit = 3 * 60 * 60;
    if ((pblock-> nTime - pindexLast->GetBlockTime()) > nLongTimeLimit){
	bnNew = bnPowLimit;
        LogPrintf("* Maximum blocktime exceeded - diff to %08x %s (bnPowLimit)\n", bnNew.GetCompact(), bnNew.ToString().c_str()); 
    }

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
    if (nActualTimespan < params.nPowTargetTimespan/4)
        nActualTimespan = params.nPowTargetTimespan/4;
    if (nActualTimespan > params.nPowTargetTimespan*4)
        nActualTimespan = params.nPowTargetTimespan*4;

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
    bnNew /= params.nPowTargetTimespan;
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

    // due to dual_kgw's timeout parameter, we won't actually require a smoothing period for new algo
    int RetargetMode = 1;

    if (pindexLast->nHeight+1 >= params.RetargetAlgorithmSwitch) { RetargetMode = 2; }
    if (RetargetMode == 1) { return GetNextWorkRequired_legacy(pindexLast, pblock, params); }
    if (RetargetMode == 2) { return DUAL_KGW3(pindexLast, pblock, params); }

    // if we're here, something weird garnon
    return GetNextWorkRequired_legacy(pindexLast, pblock, params);
}

