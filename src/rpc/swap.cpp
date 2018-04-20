
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

bool TryDecodeAtomicSwapScript(const CScript& contractRedeemscript,
                               std::vector<unsigned char>& secretHash, CKeyID& recipient, CKeyID& refund, int64_t& locktime, int64_t& secretSize)
{
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

    refundSigScript = CScript() << vch << ToByteVector(pubKey) << int64_t(0) << std::vector<unsigned char>(contractRedeemscript.begin(), contractRedeemscript.end());
    return VerifyScript(refundSigScript, contractPubKeyScript, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

bool CreateRedeemSigScript(CWallet* pwallet, const CScript& contractRedeemscript, const CScript contractPubKeyScript, const CKeyID& address, const std::vector<unsigned char>& secret, CMutableTransaction& redeemTx, CAmount amount, CScript& redeemSigScript)
{
    MutableTransactionSignatureCreator creator(pwallet, &redeemTx, 0, amount, SIGHASH_ALL|SIGHASH_FORKID);
    std::vector<unsigned char> vch;
    CPubKey pubKey;
    if (!creator.CreateSig(vch, address, pubKey, contractRedeemscript, SIGVERSION_BASE)) {
        return false;
    }

    redeemSigScript = CScript() << vch << ToByteVector(pubKey) << secret << int64_t(1) << std::vector<unsigned char>(contractRedeemscript.begin(), contractRedeemscript.end());
    return VerifyScript(redeemSigScript, contractPubKeyScript, nullptr, STANDARD_SCRIPT_VERIFY_FLAGS, creator.Checker());
}

void BuildRefundTransaction(CWallet* pwallet, const CScript& contractRedeemscript, const CMutableTransaction& contractTx, CMutableTransaction& refundTx, CAmount& refundFee)
{
    std::vector<unsigned char> secretHash;
    CKeyID recipient;
    CKeyID refundAddr;
    int64_t locktime;
    int64_t secretSize;

    if (!TryDecodeAtomicSwapScript(contractRedeemscript, secretHash, recipient, refundAddr, locktime, secretSize)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Invalid atomic swap script");
    }

    CScriptID swapContractAddr = CScriptID(contractRedeemscript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    COutPoint contractOutPoint;
    contractOutPoint.hash = contractTx.GetHash();
    for (size_t i = 0; i < contractTx.vout.size(); i++) {
        if (contractTx.vout[i].scriptPubKey.size() == contractPubKeyScript.size() &&
            std::equal(contractPubKeyScript.begin(), contractPubKeyScript.end(), contractTx.vout[i].scriptPubKey.begin())) {
            contractOutPoint.n = i;
            break;
        }
    }

    if (contractOutPoint.n == (uint32_t) -1) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Contract tx does not contain a P2SH contract payment");
    }

    //create refund transaction
    CTxDestination refundAddress = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CScript refundPubkeyScript = GetScriptForDestination(refundAddress);

    refundTx.nLockTime = locktime;
    refundTx.vout.push_back(CTxOut(0, refundPubkeyScript));

    int refundTxSize = EstimateRefundTxSerializeSize(contractRedeemscript, refundTx.vout);
    CCoinControl coinControl;
    refundFee = GetMinimumFee(refundTxSize, coinControl, ::mempool, ::feeEstimator, nullptr);
    refundTx.vout[0].nValue = contractTx.vout[contractOutPoint.n].nValue - refundFee;
    if (IsDust(refundTx.vout[0], ::dustRelayFee)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, strprintf("Redeem output value of %d is dust", refundTx.vout[0].nValue));
    }

    CTxIn txIn(contractOutPoint);
    txIn.nSequence = 0;
    refundTx.vin.push_back(txIn);

    CScript refundSigScript;
    if (!CreateRefundSigScript(pwallet, contractRedeemscript, contractPubKeyScript, refundAddr, refundTx, contractTx.vout[contractOutPoint.n].nValue, refundSigScript)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Failed to create refund script signature");
    }

    refundTx.vin[0].scriptSig = refundSigScript;
}

void BuildContract(SwapContract& contract, CWallet* pwallet, CKeyID dest, CAmount nAmount, int64_t locktime, std::vector<unsigned char> secretHash)
{
    LOCK2(cs_main, pwallet->cs_wallet);

    CTxDestination refundDest = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CKeyID refundAddress = boost::get<CKeyID>(refundDest);

    CScript contractRedeemscript = CreateAtomicSwapRedeemscript(refundAddress, dest, locktime, SECRET_SIZE, secretHash);
    CScriptID swapContractAddr = CScriptID(contractRedeemscript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

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
        throw JSONRPCError(RPC_WALLET_ERROR, strError);
    }

    CMutableTransaction refundTx;
    CAmount refundFee;
    BuildRefundTransaction(pwallet, contractRedeemscript, CMutableTransaction(*wtx.tx), refundTx, refundFee);

    contract.contractRedeemscript = contractRedeemscript;
    contract.contractAddr = swapContractAddr;
    contract.contactTx = wtx.tx;
    contract.contractFee = nFeeRequired;
    contract.refundTx = MakeTransactionRef(std::move(refundTx));
    contract.refundFee = refundFee;
}

void GenerateSecret(std::vector<unsigned char>& secret, std::vector<unsigned char>& secretHash)
{
    secret.assign(SECRET_SIZE, 0);
    secretHash.assign(CSHA256::OUTPUT_SIZE, 0);

    GetRandBytes(secret.data(), secret.size());
    CSHA256 sha;
    sha.Write(secret.data(), secret.size());
    sha.Finalize(secretHash.data());
}

UniValue SwapTxToUniv(const CTransactionRef& tx)
{
    UniValue res(UniValue::VOBJ);
    res.push_back(Pair("txid", tx->GetHash().GetHex()));
    res.push_back(Pair("hex", EncodeHexTx(*tx, RPCSerializationFlags())));
    return res;
}

void SwapContractToUniv(const SwapContract& contract, UniValue& res)
{
    UniValue c(UniValue::VOBJ);
    c.push_back(Pair("address", EncodeDestination(contract.contractAddr)));
    c.push_back(Pair("scriptHex", HexStr(contract.contractRedeemscript.begin(), contract.contractRedeemscript.end())));
    res.push_back(Pair("contract", c));

    UniValue contractData = SwapTxToUniv(contract.contactTx);
    contractData.push_back(Pair("fee", ValueFromAmount(contract.contractFee)));
    res.push_back(Pair("contractTx", contractData));

    UniValue refundData = SwapTxToUniv(contract.refundTx);
    refundData.push_back(Pair("fee", ValueFromAmount(contract.refundFee)));
    res.push_back(Pair("refundTx", refundData));
}

UniValue initiateswap(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "initiateswap \"address\" amount\n"
            "\nThe initiateswap command is performed by the initiator to create the first contract.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArgumets:\n"
            "1. \"address\"         (string, required) The recipient address\n"
            "2. amount              (numeric, required) The value for the swap\n"
            "\nResult:\n"
            "{\n"
            "  \"secret\"           (string) The hex-encoded secret for redemption\n"
            "  \"secretHash\"       (string) The hex-encoded hash of redemption secret\n" 
            "  \"contract\": {      (json object) The contract for swap \n"
            "    \"address\"        (string) The Atom address of the swap contract\n"
            "    \"scriptHex\"      (string) The contract hex script\n"
            "    }\n"
            "  \"contractTx\": {    (json object) The contract transaction\n"
            "    \"txid\"           (string) The contract transaction id encoded in little-endian hexadecimal\n"
            "    \"hex\"            (string) The hex-encoded raw transaction with signature(s)\n"
            "    \"fee\"            (numeric) The amount of the contract transaction fee in BTC\n"
            "   }\n"
            "  \"refundTx\": {      (json object) The transaction for refund\n"
            "    \"txid\"           (string) The refund transaction id encoded in little-endian hexadecimal\n"
            "    \"hex\"            (string) The hex-encoded refund transaction with signature(s)\n"
            "    \"fee\"            (numeric) The amount of the refund transaction fee in BTC\n"
            "   }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("initiateswap", "\"mh2nu899jinDFF3XEANbhWXL4RAL5JYDnv\" 1")
            + HelpExampleRpc("initiateswap", "\"mh2nu899jinDFF3XEANbhWXL4RAL5JYDnv\" 1")
        );

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (dest.which() != 1) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Participant address is not P2PKH");
    }

    CAmount nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }

    EnsureWalletIsUnlocked(pwallet);

    CKeyID destAddress = boost::get<CKeyID>(dest);
    int64_t locktime = GetAdjustedTime() + 48 * 60 * 60;
    std::vector<unsigned char> secret, secretHash;
    GenerateSecret(secret, secretHash);

    SwapContract contract;
    BuildContract(contract, pwallet, destAddress, nAmount, locktime, secretHash);

    UniValue res(UniValue::VOBJ);
    res.push_back(Pair("secret", HexStr(secret.begin(), secret.end())));
    res.push_back(Pair("secretHash", HexStr(secretHash.begin(), secretHash.end())));

    SwapContractToUniv(contract, res);

    return res;
}

UniValue auditswap(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "auditswap \"hexscript\" \"hextransaction\"\n"
            "\nThe auditswap command inspects a contract script and parses out the addresses that may claim the output, the locktime, and the secret hash. It also validates that the contract transaction pays to the contract and reports the contract output amount. Each party should audit the contract provided by the other to verify that their address is the recipient address, the output value is correct, and that the locktime is sensible.\n"
            "\nArgumets:\n"
            "1. \"hexscript\"       (string, required) The hex-encoded contract\n"
            "2. \"hextransaction\"  (string, required) The hex-encoded contract transaction\n"
            "\nResult:\n"
            "{\n"
            "  \"contractAddress\"  (string) The Atom address of the swap contract\n" 
            "  \"contractValue\"    (numeric) The value for the swap\n"
            "  \"recipientAddress\" (string) The recipient address\n"
            "  \"refundAddress\"    (string) The address for refund\n"
            "  \"secretHash\"       (string) The hash of redemption secret\n" 
            "  \"locktime\"         (numeric) The time for refund unlock\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("auditswap", "\"6382012088a82085a6637bfc298b2623b94bfeacc8d491edfa0abf3d8bb1c0f2d3b5fa58ad17158876a914109cc30524c19942078ba4356d88e6ef2aa71b546704d408da5ab17576a91423905458d1f6ed5c1efdcf87c9e4e1bc4e57a7796888ac\" \"02000000000101ddbd704186145cc9dcd0b5356d3480efe9b70cf4f2b7d9c919bc6eb506724b0100000000171600143986c86ad67c57f62174d48f159f9de554ba1138feffffff02e0d3f505000000001976a914c929fc3aad535e0d60bb416cc4294a757e31743788ac00e1f5050000000017a91471301e862d2a4e3f01bac5d9255c4d4d81331d7a8702473044022039527fbf8e0a5926d5282138617e1efaec5b2324ddf907623fb185e278b7b71e02203ca51954c1bbccd53927015a384c0236fc5ed2ed5a977adbddaa1c5f7528208a4121022e2622378dc8b9395be551ebaec071fe16ea705d9b018bdecb92ac57e6152e85742d1400\"")
            + HelpExampleRpc("auditswap", "\"6382012088a82085a6637bfc298b2623b94bfeacc8d491edfa0abf3d8bb1c0f2d3b5fa58ad17158876a914109cc30524c19942078ba4356d88e6ef2aa71b546704d408da5ab17576a91423905458d1f6ed5c1efdcf87c9e4e1bc4e57a7796888ac\" \"02000000000101ddbd704186145cc9dcd0b5356d3480efe9b70cf4f2b7d9c919bc6eb506724b0100000000171600143986c86ad67c57f62174d48f159f9de554ba1138feffffff02e0d3f505000000001976a914c929fc3aad535e0d60bb416cc4294a757e31743788ac00e1f5050000000017a91471301e862d2a4e3f01bac5d9255c4d4d81331d7a8702473044022039527fbf8e0a5926d5282138617e1efaec5b2324ddf907623fb185e278b7b71e02203ca51954c1bbccd53927015a384c0236fc5ed2ed5a977adbddaa1c5f7528208a4121022e2622378dc8b9395be551ebaec071fe16ea705d9b018bdecb92ac57e6152e85742d1400\"")
        );

    std::vector<unsigned char> scriptData(ParseHexV(request.params[0], "hexscript"));
    CScript contractRedeemscript = CScript(scriptData.begin(), scriptData.end());

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[1].get_str(), true)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Failed to decode contract transaction");
    }

    CScriptID contractAddr = CScriptID(contractRedeemscript);
    int contractOutIndex = -1;
    for (size_t i = 0; i < tx.vout.size(); i++) {
        const CTxOut& txout = tx.vout[i];
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (!ExtractDestinations(txout.scriptPubKey, type, vDest, nRequired) || type != TX_SCRIPTHASH) {
            continue;
        }

        if (*boost::get<CScriptID>(&vDest[0]) == contractAddr) {
            contractOutIndex = i;
            break;
        }
    }

    if (contractOutIndex == -1) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction does not contain the contract output");
    }

    std::vector<unsigned char> secretHash;
    CKeyID recipient;
    CKeyID refundAddr;
    int64_t locktime;
    int64_t secretSize;
    if (!TryDecodeAtomicSwapScript(contractRedeemscript, secretHash, recipient, refundAddr, locktime, secretSize)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Invalid atomic swap script");
    }

    if (secretSize != SECRET_SIZE) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, strprintf("Incorrect secret size: %d", secretSize));
    }

    UniValue res(UniValue::VOBJ);
    res.push_back(Pair("contractAddress", EncodeDestination(contractAddr)));
    res.push_back(Pair("contractValue", ValueFromAmount(tx.vout[contractOutIndex].nValue)));
    res.push_back(Pair("recipientAddress", EncodeDestination(recipient)));
    res.push_back(Pair("refundAddress", EncodeDestination(refundAddr)));
    res.push_back(Pair("secretHash", HexStr(secretHash.begin(), secretHash.end())));
    res.push_back(Pair("locktime", locktime));

    return res;
}

UniValue participateswap(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "participateswap \"address\" amount \"secrethash\"\n"
            "\nThe participateswap command is performed by the participant to create a contract on the second blockchain. It operates similarly to initiate but requires using the secret hash from the initiator's contract and creates the contract with a locktime of 24 hours.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArgumets:\n"
            "1. \"address\"         (string, required) The recipient address\n"
            "2. amount              (numeric, required) The value for the swap\n"
            "3. \"secrethash\"      (string, required) The hex-encoded hash of redemption secret\n"
            "\nResult:\n"
            "{\n"
            "  \"contract\": {      (json object) The contract for swap \n"
            "    \"address\"        (string) The Atom address of the swap contract\n"
            "    \"scriptHex\"      (string) The hex-encoded contract\n"
            "    }\n"
            "  \"contractTx\": {    (json object) The contract transaction\n"
            "    \"txid\"           (string) The contract transaction id encoded in little-endian hexadecimal\n"
            "    \"hex\"            (string) The hex-encoded contract transaction\n"
            "    \"fee\"            (numeric) The amount of the contract transaction fee in BTC\n"
            "   }\n"
            "  \"refundTx\": {      (json object) The transaction for refund\n"
            "    \"txid\"           (string) The refund transaction id encoded in little-endian hexadecimal\n"
            "    \"hex\"            (string) The hex-encoded refund transaction\n"
            "    \"fee\"            (numeric) The amount of the refund transaction fee in BTC\n"
            "   }\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("participateswap", "\"mxhBqMzeFc4N6ugGFMi5A4gRGD6yZ7HsW5\" 2 \"3fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f\"")
            + HelpExampleRpc("participateswap", "\"mxhBqMzeFc4N6ugGFMi5A4gRGD6yZ7HsW5\" 2 \"3fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f\"")
        );

    CTxDestination dest = DecodeDestination(request.params[0].get_str());
    if (dest.which() != 1) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Participant address is not P2PKH");
    }

    CAmount nAmount = AmountFromValue(request.params[1]);
    if (nAmount <= 0) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }

    std::vector<unsigned char> secretHash(ParseHexV(request.params[2], "secrethash"));
    if (secretHash.size() != CSHA256::OUTPUT_SIZE) {
         throw JSONRPCError(RPC_TYPE_ERROR, "Secret hash has wrong size");
    }

    EnsureWalletIsUnlocked(pwallet);

    CKeyID destAddress = boost::get<CKeyID>(dest);
    int64_t locktime = GetAdjustedTime() + 24 * 60 * 60;

    SwapContract contract;
    BuildContract(contract, pwallet, destAddress, nAmount, locktime, secretHash);

    UniValue res(UniValue::VOBJ);
    SwapContractToUniv(contract, res);

    return res;
}

UniValue redeemswap(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 3)
        throw std::runtime_error(
            "redeemswap \"hexscript\" \"hextransaction\" \"secret\"\n"
            "\nThe redeemswap command is performed by both parties to redeem coins paid into the contract created by the other party. Redeeming requires the secret and must be performed by the initiator first. Once the initiator's redemption has been published, the secret may be extracted from the transaction and the participant may also redeem their coins.\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArgumets:\n"
            "1. \"hexscript\"       (string, required) The hex-encoded contract\n"
            "2. \"hextransaction\"  (string, required) The hex-encoded contract transaction\n"
            "3. \"secret\"          (string, required) The hex-encoded secret for redemption\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\"             (string) The redemption transaction id encoded in little-endian hexadecimal\n" 
            "  \"hex\"              (string) The hex-encoded redemption transaction\n"
            "  \"Redeem fee\"       (numeric) The amount of the redeem transaction fee in BTC\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("redeemswap", "\"6382012088a82085a6637bfc298b2623b94bfeacc8d491edfa0abf3d8bb1c0f2d3b5fa58ad17158876a914109cc30524c19942078ba4356d88e6ef2aa71b546704d408da5ab17576a91423905458d1f6ed5c1efdcf87c9e4e1bc4e57a7796888ac\" \"02000000000101ddbd704186145cc9dcd0b5356d3480efe9b70cf4f2b7d9c919bc6eb506724b0100000000171600143986c86ad67c57f62174d48f159f9de554ba1138feffffff02e0d3f505000000001976a914c929fc3aad535e0d60bb416cc4294a757e31743788ac00e1f5050000000017a91471301e862d2a4e3f01bac5d9255c4d4d81331d7a8702473044022039527fbf8e0a5926d5282138617e1efaec5b2324ddf907623fb185e278b7b71e02203ca51954c1bbccd53927015a384c0236fc5ed2ed5a977adbddaa1c5f7528208a4121022e2622378dc8b9395be551ebaec071fe16ea705d9b018bdecb92ac57e6152e85742d1400\" \"50ab577bd16fc7e4446ac94dbf0020da01a9e31c2b3e8cccb842f5ce284f671e\"")
            + HelpExampleRpc("redeemswap", "\"6382012088a82085a6637bfc298b2623b94bfeacc8d491edfa0abf3d8bb1c0f2d3b5fa58ad17158876a914109cc30524c19942078ba4356d88e6ef2aa71b546704d408da5ab17576a91423905458d1f6ed5c1efdcf87c9e4e1bc4e57a7796888ac\" \"02000000000101ddbd704186145cc9dcd0b5356d3480efe9b70cf4f2b7d9c919bc6eb506724b0100000000171600143986c86ad67c57f62174d48f159f9de554ba1138feffffff02e0d3f505000000001976a914c929fc3aad535e0d60bb416cc4294a757e31743788ac00e1f5050000000017a91471301e862d2a4e3f01bac5d9255c4d4d81331d7a8702473044022039527fbf8e0a5926d5282138617e1efaec5b2324ddf907623fb185e278b7b71e02203ca51954c1bbccd53927015a384c0236fc5ed2ed5a977adbddaa1c5f7528208a4121022e2622378dc8b9395be551ebaec071fe16ea705d9b018bdecb92ac57e6152e85742d1400\" \"50ab577bd16fc7e4446ac94dbf0020da01a9e31c2b3e8cccb842f5ce284f671e\"")
        );

    std::vector<unsigned char> contract(ParseHexV(request.params[0], "hexscript"));

    CMutableTransaction contractTx;
    if (!DecodeHexTx(contractTx, request.params[1].get_str(), true)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Failed to decode contract transaction");
    }

    std::vector<unsigned char> secret(ParseHexV(request.params[2], "secret"));
    if (secret.size() != SECRET_SIZE) {
         throw JSONRPCError(RPC_TYPE_ERROR, "Secret has wrong size");
    }

    EnsureWalletIsUnlocked(pwallet);

    std::vector<unsigned char> secretHash;
    CKeyID recipient;
    CKeyID refund;
    int64_t locktime;
    int64_t secretSize;

    CScript contractRedeemScript(contract.begin(), contract.end());
    CScriptID swapContractAddr = CScriptID(contractRedeemScript);
    CScript contractPubKeyScript = GetScriptForDestination(swapContractAddr);

    if (!TryDecodeAtomicSwapScript(contractRedeemScript, secretHash, recipient, refund, locktime, secretSize)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Contract is not an atomic swap script recognized by this tool");
    }

    CScriptID contractAddr = CScriptID(contractRedeemScript);
    int contractOutIndex = -1;
    for (size_t i = 0; i < contractTx.vout.size(); i++) {
        const CTxOut& txout = contractTx.vout[i];
        txnouttype type;
        std::vector<CTxDestination> vDest;
        int nRequired;
        if (!ExtractDestinations(txout.scriptPubKey, type, vDest, nRequired) || type != TX_SCRIPTHASH) {
            continue;
        }

        if (*boost::get<CScriptID>(&vDest[0]) == contractAddr) {
            contractOutIndex = i;
            break;
        }
    }

    if (contractOutIndex == -1) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction does not contain the contract output");
    }

    COutPoint contractOutPoint;
    contractOutPoint.hash = contractTx.GetHash();
    contractOutPoint.n = contractOutIndex;

    LOCK2(cs_main, pwallet->cs_wallet);

    CTxDestination addr = GetRawChangeAddress(pwallet, OUTPUT_TYPE_LEGACY);
    CScript outScript = GetScriptForDestination(addr);

    CMutableTransaction redeemTx;

    redeemTx.nLockTime = locktime;
    redeemTx.vout.push_back(CTxOut(0, outScript));

    CTxIn txIn(contractOutPoint);
    redeemTx.vin.push_back(txIn);

    int redeemTxSize = EstimateRedeemTxSerializeSize(contractRedeemScript, redeemTx.vout);
    CCoinControl coinControl;
    CAmount redeemFee = GetMinimumFee(redeemTxSize, coinControl, ::mempool, ::feeEstimator, nullptr);
    redeemTx.vout[0].nValue = contractTx.vout[contractOutPoint.n].nValue - redeemFee;
    if (IsDust(redeemTx.vout[0], ::dustRelayFee)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, strprintf("Redeem output value of %d is dust", redeemTx.vout[0].nValue));
    }

    CScript redeemSigScript;
    if (!CreateRedeemSigScript(pwallet, contractRedeemScript, contractPubKeyScript, recipient, secret, redeemTx, contractTx.vout[contractOutPoint.n].nValue, redeemSigScript)) {
        throw JSONRPCError(RPC_TRANSACTION_ERROR, "Failed to create redeem script signature");
    }

    redeemTx.vin[0].scriptSig = redeemSigScript;

    UniValue res = SwapTxToUniv(MakeTransactionRef(std::move(redeemTx)));
    res.push_back(Pair("Redeem fee", ValueFromAmount(redeemFee)));

    return res;
}

UniValue extractsecret(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "extractsecret \"hextransaction\" \"secrethash\"\n"
            "\nThe extractsecret command is used by the participant to extract the secret from the initiator's redemption transaction. With the secret known, the participant may claim the coins paid into the initiator's contract.\n"
            "\nArgumets:\n"
            "1. \"hextransaction\"  (string, required) The hex-encoded redemption transaction\n"
            "2. \"secrethash\"      (string, required) The hex-encoded hash of redemption secret\n"
            "\nResult:\n"
            "{\n"
            "  \"secret\"           (string) The hex-encoded secret for redemption\n" 
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("extractsecret", "\"02000000019d06cfd5d6500bb810d726e07e4000f54076a11a3ea317ca2f2126baba95c8b600000000ef473044022001a1e090631efd01fa6a53fcc1a71b963f9096d35a8ec717771a8b0f3d240bd702203309b438b58dc990a09e820fd6d9110b331fefd5d4f2ee22c9ca4bb0b12d9daf412103089998f1ac124f16fb215f7d4cc5a428cc6acf5d7eddf1599f7a693409877f582062ad2f3179961e78e2851d5387622aa9faff6ae1266aeb5610de5f91ebb29471514c616382012088a8203fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f8876a914bc6999b404d7d393f13a242f03ebd40c17c08a676704fba4d95ab17576a91470231e7988280299558c1b5c0e83bcbe4df1c3156888acffffffff014ca8eb0b000000001976a9144536e14d69c8d3b0b090a02a2185a4238dcd226e88acfba4d95a\" \"3fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f\"")
            + HelpExampleRpc("extractsecret", "\"02000000019d06cfd5d6500bb810d726e07e4000f54076a11a3ea317ca2f2126baba95c8b600000000ef473044022001a1e090631efd01fa6a53fcc1a71b963f9096d35a8ec717771a8b0f3d240bd702203309b438b58dc990a09e820fd6d9110b331fefd5d4f2ee22c9ca4bb0b12d9daf412103089998f1ac124f16fb215f7d4cc5a428cc6acf5d7eddf1599f7a693409877f582062ad2f3179961e78e2851d5387622aa9faff6ae1266aeb5610de5f91ebb29471514c616382012088a8203fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f8876a914bc6999b404d7d393f13a242f03ebd40c17c08a676704fba4d95ab17576a91470231e7988280299558c1b5c0e83bcbe4df1c3156888acffffffff014ca8eb0b000000001976a9144536e14d69c8d3b0b090a02a2185a4238dcd226e88acfba4d95a\" \"3fbf99fcb5690013e742991b5d43f62ac897ecb8494a33feaf6f76ff16b66d9f\"")
        );

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[0].get_str(), true)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Failed to decode redemption transaction");
    }

    std::vector<unsigned char> secretHash(ParseHexV(request.params[1], "secrethash"));
    if (secretHash.size() != CSHA256::OUTPUT_SIZE) {
         throw JSONRPCError(RPC_TYPE_ERROR, "Secret hash has wrong size");
    }

    CSHA256 sha;
    for (const CTxIn txin : tx.vin) {
        CScript::const_iterator pc = txin.scriptSig.begin();
        std::vector<unsigned char> data, secretHashCalculated(CSHA256::OUTPUT_SIZE, 0);
        while (pc < txin.scriptSig.end())
        {
            opcodetype opcode;
            if (!txin.scriptSig.GetOp(pc, opcode, data)) {
                break;
            }
            if (data.size() != 0) {
                sha.Write(data.data(), data.size());
                sha.Finalize(secretHashCalculated.data());
                sha.Reset();
                if (std::equal(secretHashCalculated.begin(), secretHashCalculated.end(), secretHash.begin())) {
                    UniValue res(UniValue::VOBJ);
                    res.push_back(Pair("secret", HexStr(data.begin(), data.end())));
                    return res;
                }
            }
        }
    }

    throw JSONRPCError(RPC_TRANSACTION_ERROR, "Transaction does not contain the secret");
}

UniValue refundswap(const JSONRPCRequest& request)
{
    CWallet * const pwallet = GetWalletForJSONRPCRequest(request);
    if (!EnsureWalletIsAvailable(pwallet, request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2)
        throw std::runtime_error(
            "refundswap \"hexscript\" \"hextransaction\"\n"
            "\nThe refundswap command is used to create and send a refund of a contract transaction. While the refund transaction is created and displayed during contract creation in the initiate and participate steps, the refund can also be created after the fact in case there was any issue sending the transaction (e.g. the contract transaction was malleated or the refund fee is now too low).\n"
            + HelpRequiringPassphrase(pwallet) +
            "\nArgumets:\n"
            "1. \"hexscript\"       (string, required) The hex-encoded contract\n"
            "2. \"hextransaction\"  (string, required) The hex-encoded contract transaction\n"
            "\nResult:\n"
            "{\n"
            "  \"txid\"             (string) The redemption transaction id encoded in little-endian hexadecimal\n" 
            "  \"hex\"              (string) The hex-encoded refund transaction with signature(s)\n"
            "  \"fee\"              (numeric) The amount of the refund transaction fee in BTC\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("refundswap", "\"6382012088a82043098e913574fd73052dc4916c14641d2a7d3dc51de533e7ceb1ece447cd53018876a914bf5d762453cadfaa953ddbb334d2e9179c00c4e167041765d85ab17576a914c3c1bac44ebf6394cfc9dbaf22b13967193c2fdd6888ac\" \"02000000017d6299fc712683b1c383b15e98459561a2c1d1e554eda5ca0b6849ca96030185000000006b483045022100dfdd309af06493ae16b5f8b44bb1407639d964e4df1b7b3520b845b95731ef50022066305f28dcde0f9a28c961016408f603ce9d0d0ee1bc0ffcd250cd58baa867e141210310e244d5f11d7b6199c2e2b1099c864ec27556ff966c895612124c454c378e7ffeffffff0200b4c4040000000017a91449e1ee8259d8fe2f5fd733e0e1c7fa45cb0c8e9b87cce22607000000001976a914afbbf807011cc924043fdfc0f40ab8557f12a8dc88acdf2d1400\"")
            + HelpExampleRpc("refundswap", "\"6382012088a82043098e913574fd73052dc4916c14641d2a7d3dc51de533e7ceb1ece447cd53018876a914bf5d762453cadfaa953ddbb334d2e9179c00c4e167041765d85ab17576a914c3c1bac44ebf6394cfc9dbaf22b13967193c2fdd6888ac\" \"02000000017d6299fc712683b1c383b15e98459561a2c1d1e554eda5ca0b6849ca96030185000000006b483045022100dfdd309af06493ae16b5f8b44bb1407639d964e4df1b7b3520b845b95731ef50022066305f28dcde0f9a28c961016408f603ce9d0d0ee1bc0ffcd250cd58baa867e141210310e244d5f11d7b6199c2e2b1099c864ec27556ff966c895612124c454c378e7ffeffffff0200b4c4040000000017a91449e1ee8259d8fe2f5fd733e0e1c7fa45cb0c8e9b87cce22607000000001976a914afbbf807011cc924043fdfc0f40ab8557f12a8dc88acdf2d1400\"")
        );

    std::vector<unsigned char> scriptData(ParseHexV(request.params[0], "hexscript"));
    CScript contractRedeemscript = CScript(scriptData.begin(), scriptData.end());

    CMutableTransaction tx;
    if (!DecodeHexTx(tx, request.params[1].get_str(), true)) {
        throw JSONRPCError(RPC_DESERIALIZATION_ERROR, "Failed to decode contract transaction");
    }

    LOCK2(cs_main, pwallet->cs_wallet);

    EnsureWalletIsUnlocked(pwallet);

    CMutableTransaction refundTx;
    CAmount refundFee;
    BuildRefundTransaction(pwallet, contractRedeemscript, tx, refundTx, refundFee);

    UniValue res = SwapTxToUniv(MakeTransactionRef(std::move(refundTx)));
    res.push_back(Pair("fee", ValueFromAmount(refundFee)));

    return res;
}

static const CRPCCommand commands[] =
{ //  category              name                      actor (function)         argNames
  //  --------------------- ------------------------  -----------------------  ----------
    { "atomicswaps",    "initiateswap",     &initiateswap,      {"address","amount"} },
    { "atomicswaps",    "auditswap",        &auditswap,         {"hexscript","hextransaction"} },
    { "atomicswaps",    "participateswap",  &participateswap,   {"address","amount","secrethash"} },
    { "atomicswaps",    "redeemswap",       &redeemswap,        {"hexscript","hextransaction","secret"} },
    { "atomicswaps",    "refundswap",       &refundswap,        {"hexscript","hextransaction"} },
    { "atomicswaps",    "extractsecret",    &extractsecret,     {"hextransaction","secrethash"} },
};

void RegisterAtomicSwapRPCCommands(CRPCTable &t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++)
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
}

