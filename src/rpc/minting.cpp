#include <init.h>
#include <rpc/server.h>
#include <rpc/kernelrecord.h>
#include <pow.h>
#include <chainparams.h>
#include <validation.h>
#include <rpc/blockchain.h>
#include <wallet/rpcwallet.h>
#include <base58.h>
#include <miner.h>
#include <timedata.h>
#include <util.h>
#include <wallet/wallet.h>
#include <core_io.h>

UniValue listminting(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 4)
        throw std::runtime_error(
                "listminting count skip minweight maxweight\n"
                "1. count          (numeric, optional, default=0) The number of outputs to return (0 - all)\n"
                "2. skip           (numeric, optional, default=0) The number of outputs to skip\n"
                "3. minweight      (numeric, optional, default=0) Min output weight\n"
                "4. maxweight      (numeric, optional, default=0) Max output weight (0 - unlimited)\n"
                "Return all mintable outputs and provide details for each of them.");

    int64_t nCount = 0;
    if (!request.params[0].isNull()) {
        nCount = request.params[0].get_int64();
        if (nCount < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative count");
    }

    int64_t nSkip = 0;
    if (!request.params[1].isNull()) {
        nSkip = request.params[1].get_int64();
        if (nSkip < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative skip");
    }

    uint64_t nMinWeight = 0;
    if (!request.params[2].isNull()) {
        int64_t minWeight = request.params[2].get_int64();
        if (minWeight < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative minweight");
        nMinWeight = minWeight;
    }

    uint64_t nMaxWeight = 0;
    if (!request.params[3].isNull()) {
        int64_t maxWeight = request.params[3].get_int64();
        if (maxWeight < 0)
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Negative maxweight");
        nMaxWeight = maxWeight;
    }

    LOCK2(cs_main, pwallet->cs_wallet);
    const CBlockIndex *p = GetLastBlockIndex(chainActive.Tip(), Params().GetConsensus(), true);
    uint32_t nBits = (p == nullptr) ? UintToArith256(Params().GetConsensus().nInitialHashTargetPoS).GetCompact() : p->nBits;

    UniValue ret(UniValue::VARR);

    int minAge = Params().GetConsensus().nStakeMinAge / DAY;

    for (std::map<uint256, CWalletTx>::iterator it = pwallet->mapWallet.begin(); it != pwallet->mapWallet.end(); ++it) {
        std::vector<KernelRecord> kernelCandidates = KernelRecord::DecomposeOutputs(pwallet, it->second);
        for (KernelRecord& kr : kernelCandidates) {
            if (kr.coinAge < nMinWeight) {
                continue;
            }

            if (nMaxWeight != 0 && kr.coinAge > nMaxWeight) {
                continue;
            }

            if (nSkip != 0) {
                --nSkip;
                continue;
            }

            if (nCount != 0 && ret.size() >= (size_t)nCount) {
                break;
            }

            std::string account;
            CTxDestination adress = DecodeDestination(kr.address);
            std::map<CTxDestination, CAddressBookData>::iterator mi = pwallet->mapAddressBook.find(adress);
            if (mi != pwallet->mapAddressBook.end()) {
                account = mi->second.name;
            }

            std::string status = "immature";
            int attempts = 0;
            if (kr.GetAge() >= minAge) {
                status = "mature";
                attempts = GetAdjustedTime() - kr.nTime - Params().GetConsensus().nStakeMinAge;
            }

            UniValue obj(UniValue::VOBJ);
            obj.push_back(Pair("account",                   account));
            obj.push_back(Pair("address",                   kr.address));
            obj.push_back(Pair("txid",                      kr.hash.GetHex()));
            obj.push_back(Pair("vout",                      kr.vout));
            obj.push_back(Pair("time",                      kr.nTime));
            obj.push_back(Pair("amount",                    ValueFromAmount(kr.nValue)));
            obj.push_back(Pair("status",                    status));
            obj.push_back(Pair("age-in-day",                kr.GetAge()));
            obj.push_back(Pair("coin-day-weight",           kr.coinAge));
            obj.push_back(Pair("minting-probability-10min", kr.CalculateMintingProbabilityWithinPeriod(nBits, 10)));
            obj.push_back(Pair("minting-probability-24h",   kr.CalculateMintingProbabilityWithinPeriod(nBits, 60*24)));
            obj.push_back(Pair("minting-probability-30d",   kr.CalculateMintingProbabilityWithinPeriod(nBits, 60*24*30)));
            obj.push_back(Pair("minting-probability-90d",   kr.CalculateMintingProbabilityWithinPeriod(nBits, 60*24*90)));
            obj.push_back(Pair("attempts",                  attempts));
            ret.push_back(obj);
        }
    }

    return ret;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "minting",            "listminting",            &listminting,            {"count", "skip", "minweight", "maxweight"} },
};

void RegisterMintingRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}
