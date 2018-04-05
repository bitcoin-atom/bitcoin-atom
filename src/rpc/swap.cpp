
#include <base58.h>
#include <wallet/coincontrol.h>
#include <consensus/params.h>
#include <policy/fees.h>
#include <policy/policy.h>
#include <script/script.h>
#include <crypto/sha256.h>
#include <serialize.h>
#include <rpc/server.h>
#include <univalue/include/univalue.h>
#include <util.h>
#include <rpc/util.h>
#include <timedata.h>
#include <primitives/transaction.h>
#include <validation.h>
#include <wallet/rpcwallet.h>
#include <wallet/fees.h>
#include <core_io.h>

static const int SECRET_SIZE = 32;

class SwapContract
{
public:
    std::vector<unsigned char> secret;
    std::vector<unsigned char> secretHash;
    CScript contractRedeemscript;
    CScriptID contractAddr;
    CAmount contractFee;
    CTransactionRef contactTx;
    CTransactionRef refundTx;
    CAmount refundFee;
};


static const int redeemAtomicSwapSigScriptSize = 1 + 73 + 1 + 33 + 1 + 32 + 1;
static const int refundAtomicSwapSigScriptSize = 1 + 73 + 1 + 33 + 1;

int CalcInputSize(int scriptSigSize)
{
    return 32 + 4 + ::GetSerializeSize(VARINT(scriptSigSize), 0, 0) + scriptSigSize + 4;
}

int EstimateRefundTxSerializeSize(const CScript& contractRedeemscript, std::vector<CTxOut> txOuts)
{
    int outputsSerializeSize = 0;
    for (const CTxOut& txout : txOuts) {
        outputsSerializeSize += ::GetSerializeSize(txout, 0, 0);
    }
    return 4 + 4 + 4 + ::GetSerializeSize(VARINT(1), 0, 0) + ::GetSerializeSize(VARINT(txOuts.size()), 0, 0) + CalcInputSize(refundAtomicSwapSigScriptSize + contractRedeemscript.size()) + outputsSerializeSize;
}

int EstimateRedeemTxSerializeSize(const CScript& contractRedeemscript, std::vector<CTxOut> txOuts)
{
    int outputsSerializeSize = 0;
    for (const CTxOut& txout : txOuts) {
        outputsSerializeSize += ::GetSerializeSize(txout, 0, 0);
    }
    return 4 + 4 + 4 + ::GetSerializeSize(VARINT(1), 0, 0) + ::GetSerializeSize(VARINT(txOuts.size()), 0, 0) + CalcInputSize(redeemAtomicSwapSigScriptSize + contractRedeemscript.size()) + outputsSerializeSize;
}

bool TryDecodeAtomicSwapScript(const CScript& contractRedeemscript, const CScript& contractPubKeyScript,
                               std::vector<unsigned char>& secretHash, CScriptID& recipient, CKeyID& refund, int64_t& locktime, int64_t& secretSize)
{
    if (contractRedeemscript.size() != 20) {
        return false;
    }

    CScript::const_iterator pc = contractRedeemscript.begin();
    opcodetype opcode;
    std::vector<unsigned char> secretSizeData;
    std::vector<unsigned char> locktimeData;
    std::vector<unsigned char> recipientHash;
    std::vector<unsigned char> refundHash;

    if ((contractRedeemscript.GetOp(pc, opcode) && opcode == OP_IF) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_SIZE) &&
        (contractRedeemscript.GetOp(pc, opcode, secretSizeData)) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_SHA256) &&
        (contractRedeemscript.GetOp(pc, opcode, secretHash) && opcode == 0x20) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_DUP) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_HASH160) &&
        (contractRedeemscript.GetOp(pc, opcode, recipientHash) && opcode == 0x14) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_ELSE) &&
        (contractRedeemscript.GetOp(pc, opcode, locktimeData)) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_CHECKLOCKTIMEVERIFY) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_DROP) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_DUP) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_HASH160) &&
        (contractRedeemscript.GetOp(pc, opcode, refundHash) && opcode == 0x14) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_ENDIF) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_EQUALVERIFY) &&
        (contractRedeemscript.GetOp(pc, opcode) && opcode == OP_CHECKSIG)
        ) {
        secretSize = CScriptNum(secretSizeData, true).getint();
        locktime = CScriptNum(locktimeData, true).getint();
        recipient = CKeyID(uint160(recipientHash));
        refund = CKeyID(uint160(refundHash));
        return true;
    }

    return false;
}

bool CreateRefundSigScript(CWallet* pwallet, const CScript& contractRedeemscript, const CScript contractPubKeyScript, const CKeyID& address, CMutableTransaction& refundTx, CAmount amount, CScript& refundSigScript)
{
    MutableTransactionSignatureCreator creator(pwallet, &refundTx, 0, amount, SIGHASH_ALL|SIGHASH_FORKID);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    if (!creator.CreateSig(vch, address, pubKey, contractRedeemscript, SIGVERSION_BASE)) {
        return false;
    }

    refundSigScript = CScript() << vch << ToByteVector(pubKey) << 0;
    refundSigScript += contractRedeemscript;
    return VerifyScript(refundSigScript, contractPubKeyScript, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

bool BuildRefundTransaction(CWallet* pwallet, const CScript& contractRedeemscript, const CMutableTransaction& contractTx, CMutableTransaction& refundTx)
{
    CScriptID swapContractAddr = CScriptID(contractRedeemscript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    COutPoint contractOutPoint;
    contractOutPoint.hash = contractTx.GetHash();
    for (size_t i = 0; i < contractTx.vout.size(); i++) {
        if (contractTx.vout[i].scriptPubKey.size() == contractPubKeyScript.size() &&
            std::equal(contractPubKeyScript.begin(), contractPubKeyScript.end(), contractTx.vout[i].scriptPubKey.begin())) {
            contractOutPoint.n = i;
        }
    }

    if (contractOutPoint.n == (uint32_t) -1) {
        return false;
    }

    std::vector<unsigned char> secretHash;
    CScriptID recipient;
    CKeyID refundAddr;
    int64_t locktime;
    int64_t secretSize;

    if (!TryDecodeAtomicSwapScript(contractRedeemscript, contractPubKeyScript, secretHash, recipient, refundAddr, locktime, secretSize)) {
        return false;
    }

    //create refund transaction
    CTxDestination refundAddress = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CScript refundPubkeyScript = GetScriptForDestination(refundAddress);

    refundTx.nLockTime = locktime;
    refundTx.vout.push_back(CTxOut(0, refundPubkeyScript));

    int refundTxSize = EstimateRefundTxSerializeSize(contractRedeemscript, refundTx.vout);
    CCoinControl coinControl;
    CAmount refundFee = GetMinimumFee(refundTxSize, coinControl, ::mempool, ::feeEstimator, nullptr);
    refundTx.vout[0].nValue = contractTx.vout[contractOutPoint.n].nValue - refundFee;
    if (IsDust(refundTx.vout[0], ::dustRelayFee)) {
        return false;
    }

    CTxIn txIn(contractOutPoint);
    txIn.nSequence = 0;
    refundTx.vin.push_back(txIn);

    CScript refundSigScript;
    if (!CreateRefundSigScript(pwallet, contractRedeemscript, contractPubKeyScript, refundAddr, refundTx, contractTx.vout[contractOutPoint.n].nValue, refundSigScript)) {
        return false;
    }

    refundTx.vin[0].scriptSig = refundSigScript;
    return true;
}

bool BuildContract(SwapContract& contract, CWallet* pwallet, CKeyID dest, CAmount nAmount)
{
    unsigned char secret[SECRET_SIZE];
    unsigned char secretHash[CSHA256::OUTPUT_SIZE];

    //TODO: initialize secret (random)

    CSHA256 sha;
    sha.Write(secret, SECRET_SIZE);
    sha.Finalize(secretHash);

    //build contract
    CTxDestination refundDest = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CKeyID refundAddress = boost::get<CKeyID>(refundDest);

    int64_t locktime = GetAdjustedTime() + 48 * 60 * 60;

    std::vector<unsigned char> vSecretHash;
    vSecretHash.insert(vSecretHash.end(), secretHash, secretHash + CSHA256::OUTPUT_SIZE);
    CScript contractRedeemscript = CreateAtomicSwapRedeemscript(refundAddress, dest, locktime, SECRET_SIZE, vSecretHash);
    CScriptID swapContractAddr = CScriptID(contractRedeemscript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    // Create the contract transaction
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {contractPubKeyScript, nAmount, false};
    vecSend.push_back(recipient);
    CCoinControl coinControl;
    coinControl.change_type = OUTPUT_TYPE_LEGACY;
    CWalletTx wtx;
    if (!pwallet->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, coinControl)) {
        return false;
    }

    // build a refund tx
    CMutableTransaction refundTx;
    if (!BuildRefundTransaction(pwallet, contractRedeemscript, CMutableTransaction(*wtx.tx), refundTx)) {
        return false;
    }

    contract = SwapContract();
    contract.secret = std::vector<unsigned char>(secret, secret + SECRET_SIZE);
    contract.secretHash = vSecretHash;
    contract.contractRedeemscript = contractRedeemscript;
    contract.contractAddr = swapContractAddr;
    contract.contactTx = wtx.tx;
    contract.refundTx = MakeTransactionRef(CTransaction(refundTx));
    return true;
}

UniValue SwapTxToUniv(const CTransactionRef& tx)
{
    UniValue res(UniValue::VOBJ);
    res.push_back(Pair("txid", tx->GetHash().GetHex()));
    res.push_back(Pair("hex", EncodeHexTx(*tx, RPCSerializationFlags())));
    return res;
}

UniValue initiateswap(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error("");

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (dest.which() != 1) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Participant address is not P2PKH");
    }

    CAmount nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0)
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");

    CKeyID destAddress = boost::get<CKeyID>(dest);
    SwapContract contract;
    if (!BuildContract(contract, pwallet, destAddress, nAmount)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "");
    }

    UniValue res(UniValue::VOBJ);

    res.push_back(Pair("secret", HexStr(contract.secret.begin(), contract.secret.end())));
    res.push_back(Pair("secretHash", HexStr(contract.secretHash.begin(), contract.secretHash.end())));

    UniValue c(UniValue::VOBJ);
    c.push_back(Pair("address", EncodeDestination(contract.contractAddr)));
    c.push_back(Pair("scriptHex", HexStr(contract.contractRedeemscript.begin(), contract.contractRedeemscript.end())));
    res.push_back(Pair("contract", c));

    UniValue contractData = SwapTxToUniv(contract.contactTx);
    contractData.push_back(Pair("fee", contract.contractFee));
    res.push_back(Pair("contractTx", contractData));

    UniValue refundData = SwapTxToUniv(contract.refundTx);
    refundData.push_back(Pair("fee", contract.refundFee));
    res.push_back(Pair("refundTx", refundData));

    return res;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "atomicswaps",    "initiateswap",      &initiateswap,      {"address","amount"} },
};

void RegisterAtomicSwapRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

