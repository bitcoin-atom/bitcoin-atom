#ifndef BITCOIN_RPC_SWAP_H
#define BITCOIN_RPC_SWAP_H

#include <rpc/protocol.h>
#include <amount.h>
#include <script/standard.h>
#include <script/script.h>

class CWallet;
class CTransaction;
class CCoinControl;
class CReserveKey;

struct CMutableTransaction;

typedef std::shared_ptr<const CTransaction> CTransactionRef;

class SwapContract
{
public:
    CScript contractRedeemscript;
    CScriptID contractAddr;
    CAmount contractFee;
    CTransactionRef contactTx;
    CTransactionRef refundTx;
    CAmount refundFee;
};

bool initiateswap(CWallet* const pwallet, const CCoinControl& coinControl, const std::string& destination, CAmount nAmount, std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash, SwapContract& contract, CAmount& contractFee, CReserveKey& reservekey, RPCErrorCode& error, std::string& errorStr);
bool participateswap(CWallet* const pwallet, const CCoinControl& coinControl, const std::string& destination, CAmount nAmount, const std::string& secretHashStr, SwapContract& contract, CReserveKey& reservekey, RPCErrorCode& error, std::string& errorStr);
bool redeemswap(CWallet* const pwallet, const CCoinControl& coinControl, const std::string& contractStr, const std::string& contractTxStr, const std::string& secretStr, CMutableTransaction& redeemTx, CAmount& redeemFee, RPCErrorCode& error, std::string& errorStr);
bool extractsecret(const std::string& redeemTxStr, const std::string& secretHashStr, std::vector<unsigned char>& data, RPCErrorCode& error, std::string& errorStr);
bool refundswap(CWallet* const pwallet, const std::string& contractStr, const std::string& contractTxStr, CMutableTransaction& refundTx, CAmount& refundFee, RPCErrorCode& error, std::string& errorStr);

#endif