// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_POW_H
#define BITCOIN_POW_H

#include <arith_uint256.h>
#include <consensus/params.h>

#include <stdint.h>

class CBlockHeader;
class CBlockIndex;
class uint256;

const CBlockIndex* GetLastBlockIndex(const CBlockIndex* pindex, const Consensus::Params& params, bool fProofOfStake);

uint32_t GetNextWorkRequired(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params, bool fProofOfStake);
uint32_t GetNextWorkRequiredForPow(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params);
uint32_t CalculateNextWorkRequired(arith_uint256 bnAvg, int64_t nLastBlockTime, int64_t nFirstBlockTime, const Consensus::Params&);
uint32_t GetNextWorkRequiredForPowOld(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params);
uint32_t CalculateNextWorkRequiredOld(const CBlockIndex* pindexLast, int64_t nFirstBlockTime, const Consensus::Params&);
uint32_t GetNextWorkRequiredForPos(const CBlockIndex* pindexLast, const CBlockHeader *pblock, const Consensus::Params& params);

/** Check whether a block hash satisfies the proof-of-work requirement specified by nBits */
bool CheckProofOfWork(uint256 hash, unsigned int nBits, const Consensus::Params&);

#endif // BITCOIN_POW_H
