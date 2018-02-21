#include <rpc/kernelrecord.h>
#include <wallet/wallet.h>
#include <base58.h>
#include <timedata.h>
#include <chainparams.h>
#include <math.h>
#include <txdb.h>
#include <validation.h>


bool KernelRecord::ShowTransaction(const CWalletTx &wtx)
{
    int nDepth = wtx.GetDepthInMainChain();
    if (wtx.IsCoinBase()) {
        if (nDepth < 2) {
            return false;
        }
    } else if (nDepth == 0) {
        return false;
    }

    return true;
}

/*
 * Decompose CWallet transaction to model kernel records.
 */
std::vector<KernelRecord> KernelRecord::DecomposeOutputs(const CWallet *wallet, const CWalletTx &wtx)
{
    std::vector<KernelRecord> kernels;

    int64_t nTime = 0;
    uint256 hash = wtx.GetHash();

    CDiskTxPos postx;
    if (pblocktree->ReadTxIndex(hash, postx))
    {
        CAutoFile file(OpenBlockFile(postx, true), SER_DISK, CLIENT_VERSION);
        CBlockHeader header;
        try {
            file >> header;
            nTime = header.GetBlockTime();
        } catch (std::exception &e) {
            return kernels;
        }
    }

    std::map<std::string, std::string> mapValue = wtx.mapValue;
    int nDayWeight = (std::min((GetAdjustedTime() - nTime), Params().GetConsensus().nStakeMaxAge) - Params().GetConsensus().nStakeMinAge) / DAY;

    if (ShowTransaction(wtx)) {
        size_t voutCount = wtx.tx ? wtx.tx->vout.size() : 0;
        for (size_t nOut = 0; nOut < voutCount; nOut++) {
            CTxOut txOut = wtx.tx->vout[nOut];
            if (wallet->IsMine(txOut)) {
                CTxDestination address;
                std::string addrStr;

                if (ExtractDestination(txOut.scriptPubKey, address)) {
                    // Sent to Bitcoin Address
                    addrStr = EncodeDestination(address);
                } else {
                    // Sent to IP, or other non-address transaction like OP_EVAL
                    addrStr = mapValue["to"];
                }

                uint64_t coinAge = std::max(txOut.nValue * nDayWeight / COIN, (int64_t)0);
                kernels.push_back(KernelRecord(hash, nTime, addrStr, txOut.nValue, nOut, wallet->IsSpent(wtx.GetHash(), nOut), coinAge));
            }
        }
    }

    return kernels;
}

std::string KernelRecord::GetTxID() const
{
    return hash.ToString() + strprintf("-%03d", vout);
}

int64_t KernelRecord::GetAge() const
{
    return (GetAdjustedTime() - nTime) / DAY;
}

double KernelRecord::CalcMintingProbability(uint32_t nBits, int timeOffset) const
{
    int64_t nTimeWeight = std::min((GetAdjustedTime() - nTime) + timeOffset, Params().GetConsensus().nStakeMaxAge) - Params().GetConsensus().nStakeMinAge;
    arith_uint256 bnCoinDayWeight = arith_uint256(nValue) * nTimeWeight / COIN / DAY;

    arith_uint256 bnTargetPerCoinDay;
    bnTargetPerCoinDay.SetCompact(nBits);

    const double targetLimit = (~arith_uint256(0)).getdouble();
    return (bnCoinDayWeight * bnTargetPerCoinDay).getdouble() / targetLimit;
}

double KernelRecord::CalculateMintingProbabilityWithinPeriod(uint32_t nBits, int minutes) const
{
    if (nBits != prevBits || minutes != prevMinutes) {
        double prob = 1;
        double p;
        int d = minutes / (60 * 24); // Number of full days
        int m = minutes % (60 * 24); // Number of minutes in the last day
        int timeOffset = DAY;

        // Probabilities for the first d days
        for(int i = 0; i < d; i++, timeOffset += DAY) {
            p = pow(1 - CalcMintingProbability(nBits, timeOffset), DAY);
            prob *= p;
        }

        // Probability for the m minutes of the last day
        p = pow(1 - CalcMintingProbability(nBits, timeOffset), 60 * m);
        prob *= p;

        prob = 1 - prob;
        prevBits = nBits;
        prevProbability = prob;
        prevMinutes = minutes;
    }
    return prevProbability;
}
