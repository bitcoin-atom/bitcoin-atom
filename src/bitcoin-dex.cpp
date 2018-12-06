#include <iostream>
#include <fs.h>
#include <string>
#include "util.h"
#include "wallet/wallet.h"
#include "wallet/init.h"
#include "init.h"
#include "chainparamsbase.h"
#include "chainparams.h"
#include "base58.h"
#include "validation.h"
#include "script/standard.h"
#include "hash.h"
#include "primitives/transaction.h"
#include "core_io.h"
#include "wallet/coincontrol.h"
#include "policy/feerate.h"
#include "keystore.h"
#include "rpc/swap.h"

void endCommand(int command) {
    std::cout << "end " << command << std::endl;
}

void endCommandWithError(int command, const std::string& error) {
    std::cout << "end " << command << " error " << error << std::endl;
}

void endCommandWithResult(int command, const std::string& result, long long value) {
    std::cout << "end " << command << " " << result << " " << value << std::endl;
}

void endCommandWithResult(int command, const std::string& result, long long value1, long long value2) {
    std::cout << "end " << command << " " << result << " " << value1 << " " << value2 << std::endl;
}

void endCommandWithResult(int command, const std::string& result1) {
    std::cout << "end " << command << " " << result1 << std::endl;
}

void endCommandWithResult(int command, const std::string& result1, const std::string& result2) {
    std::cout << "end " << command << " " << result1 << " " << result2 << std::endl;
}

void endCommandWithResult(int command, const std::string& result1, const std::string& result2, const std::string& result3) {
    std::cout << "end " << command << " " << result1 << " " << result2 << " " << result3 << std::endl;
}

void endCommandWithResult(int command, const std::string& result1, const std::string& result2, const std::string& result3, const std::string& result4) {
    std::cout << "end " << command << " " << result1 << " " << result2 << " " << result3 << " " << result4 << std::endl;
}

void endCommandWithResult(int command, const std::string& result1, const std::string& result2, const std::string& result3, const std::string& result4, long long value1, long long value2) {
    std::cout << "end " << command << " " << result1 << " " << result2 << " " << result3 << " " << result4 << " " << value1 << " " << value2 << std::endl;
}

void setDataDir() {
	std::string path;
#ifdef WIN32
    // Windows
    char pszPath[MAX_PATH] = "";

    if(SHGetSpecialFolderPathA(nullptr, pszPath, CSIDL_APPDATA, true)) {
        path = pszPath;
    } else {
    	path = "";
    }

    path += "/Tritium_bca";
#else
    char* pszHome = getenv("HOME");
    if (pszHome == nullptr || strlen(pszHome) == 0)
        path = "";
    else
        path = pszHome;
#ifdef MAC_OSX
    // Mac
    path += "/Library/Application Support/Tritium_bca";
#else
    // Unix
    path += "/.tritium_bca";
#endif
#endif

    gArgs.SoftSetArg("-datadir", path);
}

std::string static EncodeDumpString(const std::string &str) {
    std::stringstream ret;
    for (unsigned char c : str) {
        if (c <= 32 || c >= 128 || c == '%') {
            ret << '%' << HexStr(&c, &c + 1);
        } else {
            ret << c;
        }
    }
    return ret.str();
}

bool GetWalletAddressesForKey(CWallet * const pwallet, const CKeyID &keyid, std::string &strAddr, std::string &strLabel)
{
    bool fLabelFound = false;
    CKey key;
    pwallet->GetKey(keyid, key);
    for (const auto& dest : GetAllDestinationsForKey(key.GetPubKey())) {
        if (pwallet->mapAddressBook.count(dest)) {
            if (!strAddr.empty()) {
                strAddr += ",";
            }
            strAddr += EncodeDestination(dest);
            strLabel = EncodeDumpString(pwallet->mapAddressBook[dest].name);
            fLabelFound = true;
        }
    }
    if (!fLabelFound) {
        strAddr = EncodeDestination(GetDestinationForKey(key.GetPubKey(), g_address_type));
    }
    return fLabelFound;
}

void getKeys(CWallet* pwallet) {
    LOCK2(cs_main, pwallet->cs_wallet);

    std::map<CTxDestination, int64_t> mapKeyBirth;
    const std::map<CKeyID, int64_t>& mapKeyPool = pwallet->GetAllReserveKeys();
    pwallet->GetKeyBirthTimes(mapKeyBirth);

    std::vector<std::pair<int64_t, CKeyID> > vKeyBirth;
    for (const auto& entry : mapKeyBirth) {
        if (const CKeyID* keyID = boost::get<CKeyID>(&entry.first)) {
            vKeyBirth.push_back(std::make_pair(entry.second, *keyID));
        }
    }

    CKeyID masterKeyID = pwallet->GetHDChain().masterKeyID;

    for (std::vector<std::pair<int64_t, CKeyID> >::const_iterator it = vKeyBirth.begin(); it != vKeyBirth.end(); it++) {
        const CKeyID &keyid = it->second;
        std::string strAddr;
        std::string strLabel;
        CKey key;
        if (pwallet->GetKey(keyid, key)) {
            std::string strAddrLegacy = EncodeDestination(GetDestinationForKey(key.GetPubKey(), OUTPUT_TYPE_LEGACY));
            if (GetWalletAddressesForKey(pwallet, keyid, strAddr, strLabel)) {
                std::cout << "1 " << CBitcoinSecret(key).ToString() << " " << strAddr << " " << strAddrLegacy << std::endl;
            } else if (keyid == masterKeyID) {

            } else if (mapKeyPool.count(keyid)) {

            } else if (pwallet->mapKeyMetadata[keyid].hdKeypath == "m") {

            } else {
                std::cout << "1 " << CBitcoinSecret(key).ToString() << " " << strAddr << " " << strAddrLegacy << std::endl;
            }
        }
    }

    endCommand(1);
}

void addTransaction(CWallet* pwallet, const std::string& transaction) {
    CMutableTransaction tx;
    if (!DecodeHexTx(tx, transaction, true, true)) {
        endCommandWithError(2, "failed parse tx");
    } else {
        uint256 transactionHash = tx.GetHash();
        CWalletTx walletTx(pwallet, MakeTransactionRef(std::move(tx)));
        pwallet->mapWallet[transactionHash] = walletTx;
        endCommand(2);
    }
}

void clearTransactions(CWallet* pwallet) {
    pwallet->mapWallet.clear();
    endCommand(3);
}

void createTransaction(CWallet* pwallet, const std::string& recepientAddress, long long value, long long feePerKb) {
    CReserveKey reservekey(pwallet);
    CAmount nFeeRequired;
    std::string strError;
    std::vector<CRecipient> vecSend;
    int nChangePosRet = -1;
    CRecipient recipient = {GetScriptForDestination(DecodeDestination(recepientAddress)), value, false};
    vecSend.push_back(recipient);
    CCoinControl ctrl;
    ctrl.change_type = OUTPUT_TYPE_LEGACY;
    ctrl.m_feerate = CFeeRate(feePerKb);
    CWalletTx wtx;
    if (!pwallet->CreateTransaction(vecSend, wtx, reservekey, nFeeRequired, nChangePosRet, strError, ctrl)) {
        endCommandWithError(4, strError);
    } else {
        reservekey.KeepKey();
        std::string txHex = EncodeHexTx(*(wtx.tx));
        endCommandWithResult(4, txHex, value, nFeeRequired);
    }
}

void getNewAddress(CWallet* pwallet) {
    CReserveKey reservekey(pwallet);
    CPubKey vchPubKey;
    if (!reservekey.GetReservedKey(vchPubKey, true)) {
        endCommandWithError(5, "Error: Keypool ran out, please call keypoolrefill first");
    } else {
        reservekey.KeepKey();
        pwallet->LearnRelatedScripts(vchPubKey, g_address_type);
        CKeyID keyId = vchPubKey.GetID();
        CKey vchSecret;
        if (!pwallet->GetKey(keyId, vchSecret)) {
            endCommandWithError(5, "Saving new key failed");
        } else {
            CTxDestination address = GetDestinationForKey(vchPubKey, g_address_type);
            CTxDestination addressLegacy = GetDestinationForKey(vchPubKey, OUTPUT_TYPE_LEGACY);
            endCommandWithResult(5, CBitcoinSecret(vchSecret).ToString(), EncodeDestination(address), EncodeDestination(addressLegacy));
        }
    }
}

void initiateSwap(CWallet* pwallet, const std::string& address, long long value, long long feePerKb) {
    std::vector<unsigned char> secret, secretHash;
    SwapContract contract;
    RPCErrorCode error;
    std::string errorStr;

    CCoinControl coinControl;
    coinControl.change_type = OUTPUT_TYPE_LEGACY;
    coinControl.m_feerate = CFeeRate(feePerKb);

    CReserveKey reservekey(pwallet);

    CAmount contractFee = 0;

    if (!initiateswap(pwallet, coinControl, address, value, secret, secretHash, contract, contractFee, reservekey, error, errorStr)) {
        endCommandWithError(6, errorStr);
    } else {
        reservekey.KeepKey();
        std::string contractStr = HexStr(contract.contractRedeemscript.begin(), contract.contractRedeemscript.end());
        std::string contractTxStr = EncodeHexTx(*(contract.contactTx));
        std::string secretStr = HexStr(secret.begin(), secret.end());
        std::string secretHashStr = HexStr(secretHash.begin(), secretHash.end());
        endCommandWithResult(6, contractStr, contractTxStr, secretStr, secretHashStr, value, contractFee);
    }
}

void participateSwap(CWallet* pwallet, const std::string& address, const std::string& secretHash, long long value, long long feePerKb) {
    SwapContract contract;
    RPCErrorCode error;
    std::string errorStr;

    CCoinControl coinControl;
    coinControl.change_type = OUTPUT_TYPE_LEGACY;
    coinControl.m_feerate = CFeeRate(feePerKb);

    CReserveKey reservekey(pwallet);

    if (!participateswap(pwallet, coinControl, address, value, secretHash, contract, reservekey, error, errorStr)) {
        endCommandWithError(7, errorStr);
    } else {
        reservekey.KeepKey();
        std::string contractStr = HexStr(contract.contractRedeemscript.begin(), contract.contractRedeemscript.end());
        std::string contractTxStr = EncodeHexTx(*(contract.contactTx));
        endCommandWithResult(7, contractStr, contractTxStr);
    }
}

void extractSecret(const std::string& redeemTx, const std::string& secretHash) {
    std::vector<unsigned char> secret;
    RPCErrorCode error;
    std::string errorStr;

    if (!extractsecret(redeemTx, secretHash, secret, error, errorStr)) {
        endCommandWithError(8, errorStr);
    } else {
        std::string secretStr = HexStr(secret.begin(), secret.end());
        endCommandWithResult(8, secretStr);
    }
}

void redeemSwap(CWallet* pwallet, const std::string& contract, const std::string& contractTx, const std::string& secret, long long feePerKb) {
    CMutableTransaction redeemTx;
    CAmount redeemFee;
    RPCErrorCode error; 
    std::string errorStr;

    CCoinControl coinControl;
    coinControl.m_feerate = CFeeRate(feePerKb);

    if (!redeemswap(pwallet, coinControl, contract, contractTx, secret, redeemTx, redeemFee, error, errorStr)) {
        endCommandWithError(9, errorStr);
    } else {
        std::string txHex = EncodeHexTx(*(MakeTransactionRef(std::move(redeemTx))));
        endCommandWithResult(9, txHex, redeemFee);
    }
}

void refundSwap(CWallet* pwallet, const std::string& contract, const std::string& contractTx, long long feePerKb) {
    CMutableTransaction refundTx;
    CAmount refundFee;
    RPCErrorCode error; 
    std::string errorStr;

    if (!refundswap(pwallet, contract, contractTx, refundTx, refundFee, error, errorStr)) {
        endCommandWithError(10, errorStr);
    } else {
        std::string txHex = EncodeHexTx(*(MakeTransactionRef(std::move(refundTx))));
        endCommandWithResult(10, txHex, refundFee);
    }
}

int main(int argc, char *argv[])
{
	gArgs.ParseParameters(argc, argv);
	setDataDir();

    if (!fs::is_directory(GetDataDir(false))) {
    	fs::path path = fs::system_complete(gArgs.GetArg("-datadir", ""));
    	if (fs::create_directories(path)) {
        	fs::create_directories(path / "wallets");
    	} else {
        	//std::cout << "Error: Specified data directory \"" << gArgs.GetArg("-datadir", "") << "\" does not exist.\n" << std::endl;
        	return 1;
    	}
     }

    SelectParams(ChainNameFromCommandLine());

    if (!AppInitParameterInteraction()) {
        return 1;
    }

    if (!AppInitSanityChecks()) {
        return 1;
    }

    if (!AppInitLockDataDirectory()) {
        return 1;
    }

    CWallet* wallet = CWallet::CreateSimpleWalletFromFile("tritium.dat");
    if (!wallet) {
    	//std::cout << "Error loading wallet" << std::endl;
    }

    int command = -1;
    while (std::cin >> command && command) {
        if (command == 1) {
            getKeys(wallet);
        }
        if (command == 2) {
            std::cout << "2" << std::endl;
            std::string transaction;
            std::cin >> transaction;
            addTransaction(wallet, transaction);
        }
        if (command == 3) {
            clearTransactions(wallet);
        }
        if (command == 4) {
            std::cout << "4 1" << std::endl;
            std::string recepientAddress;
            std::cin >> recepientAddress;
            std::cout << "4 2" << std::endl;
            long long value;
            std::cin >> value;
            std::cout << "4 3" << std::endl;
            long long feePerKb;
            std::cin >> feePerKb;
            createTransaction(wallet, recepientAddress, value, feePerKb);
        }
        if (command == 5) {
            getNewAddress(wallet);
        }
        if (command == 6) {
            std::cout << "6 1" << std::endl;
            std::string address;
            std::cin >> address;
            std::cout << "6 2" << std::endl;
            long long value;
            std::cin >> value;
            std::cout << "6 3" << std::endl;
            long long feePerKb;
            std::cin >> feePerKb;
            initiateSwap(wallet, address, value, feePerKb);
        }
        if (command == 7) {
            std::cout << "7 1" << std::endl;
            std::string address;
            std::cin >> address;
            std::cout << "7 2" << std::endl;
            std::string secretHash;
            std::cin >> secretHash;
            std::cout << "7 3" << std::endl;
            long long value;
            std::cin >> value;
            std::cout << "7 4" << std::endl;
            long long feePerKb;
            std::cin >> feePerKb;
            participateSwap(wallet, address, secretHash, value, feePerKb);
        }
        if (command == 8) {
            std::cout << "8 1" << std::endl;
            std::string redeemTx;
            std::cin >> redeemTx;
            std::cout << "8 2" << std::endl;
            std::string secretHash;
            std::cin >> secretHash;
            extractSecret(redeemTx, secretHash);
        }
        if (command == 9) {
            std::cout << "9 1" << std::endl;
            std::string contract;
            std::cin >> contract;
            std::cout << "9 2" << std::endl;
            std::string contractTx;
            std::cin >> contractTx;
            std::cout << "9 3" << std::endl;
            std::string secret;
            std::cin >> secret;
            std::cout << "9 4" << std::endl;
            long long feePerKb;
            std::cin >> feePerKb;
            redeemSwap(wallet, contract, contractTx, secret, feePerKb);
        }
        if (command == 10) {
            std::cout << "10 1" << std::endl;
            std::string contract;
            std::cin >> contract;
            std::cout << "10 2" << std::endl;
            std::string contractTx;
            std::cin >> contractTx;
            std::cout << "10 3" << std::endl;
            long long feePerKb;
            std::cin >> feePerKb;
            refundSwap(wallet, contract, contractTx, feePerKb);
        }
    }

    wallet->Flush(true);

	return 0;
}