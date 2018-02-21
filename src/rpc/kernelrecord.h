#ifndef KERNELRECORD_H
#define KERNELRECORD_H

#include <amount.h>
#include <uint256.h>

const int DAY = 24 * 60 * 60;

class CWallet;
class CWalletTx;

class KernelRecord
{
public:
    uint256 hash;
    int64_t nTime;
    std::string address;
    CAmount nValue;
    int vout;
    bool spent;
    uint64_t coinAge;

    KernelRecord():
        hash(), nTime(0), address(""), nValue(0), vout(0), spent(false), coinAge(0), prevMinutes(0), prevBits(0), prevProbability(0)
    {
    }

    KernelRecord(uint256 hash, int64_t nTime):
            hash(hash), nTime(nTime), address(""), nValue(0), vout(0), spent(false), coinAge(0), prevMinutes(0), prevBits(0), prevProbability(0)
    {
    }

    KernelRecord(uint256 hash, int64_t nTime,
                 const std::string &address,
                 CAmount nValue, int idx, bool spent, int64_t coinAge):
        hash(hash), nTime(nTime), address(address), nValue(nValue),
        vout(idx), spent(spent), coinAge(coinAge), prevMinutes(0), prevBits(0), prevProbability(0)
    {
    }

    static bool ShowTransaction(const CWalletTx &wtx);
    static std::vector<KernelRecord> DecomposeOutputs(const CWallet *wallet, const CWalletTx &wtx);

    std::string GetTxID() const;
    int64_t GetAge() const;
    double CalcMintingProbability(uint32_t nBits, int timeOffset = 0) const;
    double CalculateMintingProbabilityWithinPeriod(uint32_t nBits, int minutes) const;
protected:
    mutable int prevMinutes;
    mutable uint32_t prevBits;
    mutable double prevProbability;
};

#endif // KERNELRECORD_H
