#include <chainparams.h>
#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <validation.h>
#include <miner.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <script/standard.h>
#include <txmempool.h>
#include <uint256.h>
#include <util.h>
#include <utilstrencodings.h>
#include <wallet/wallet.h>
#include <utiltime.h>
#include <wallet/test/wallet_test_fixture.h>
#include <wallet/coincontrol.h>
#include <test/test_bitcoin.h>

#include <memory>

#include <boost/test/unit_test.hpp>

static void AddKey(CWallet& wallet, const CKey& key)
{
    LOCK(wallet.cs_wallet);
    wallet.AddKeyPubKey(key, key.GetPubKey());
}

class CoinStakeTestingSetup : public TestChain100Setup
{
public:
    CoinStakeTestingSetup() : TestChain100Setup(true)
    {
        CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        ::bitdb.MakeMock();
        g_address_type = OUTPUT_TYPE_DEFAULT;
        g_change_type = OUTPUT_TYPE_DEFAULT;
        wallet.reset(new CWallet(std::unique_ptr<CWalletDBWrapper>(new CWalletDBWrapper(&bitdb, "wallet_test.dat"))));
        wallet->SetMaxVersion(FEATURE_LATEST);
        bool firstRun;
        wallet->LoadWallet(firstRun);
        AddKey(*wallet, coinbaseKey);

        RescanWalletTransactions();
    }

    ~CoinStakeTestingSetup()
    {
        wallet.reset();
        ::bitdb.Flush(true);
        ::bitdb.Reset();
    }

    CWalletTx& AddTx(CRecipient recipient)
    {
        CWalletTx wtx;
        CReserveKey reservekey(wallet.get());
        CAmount fee;
        int changePos = -1;
        std::string error;
        CCoinControl dummy;
        BOOST_CHECK(wallet->CreateTransaction({recipient}, wtx, reservekey, fee, changePos, error, dummy));
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(wtx, reservekey, nullptr, state));

        return MineBlockWithTx(wtx);
    }

    CWalletTx& Transfer(const std::set<CInputCoin> &inputCoins, const CTxDestination &destination)
    {
        CAmount totalValue = 0;
        for (const auto& coin : inputCoins) {
            totalValue += coin.txout.nValue;
        }

        CScript newDestScriptPubKey = GetScriptForDestination(destination);

        // For the sake of simplicity, just set the fee amount definitely enough
        CAmount fee = 1 * COIN;
        BOOST_CHECK(totalValue - fee > 0);

        CMutableTransaction txNew;

        const uint32_t nSequence = CTxIn::SEQUENCE_FINAL - 1;
        for (const auto& coin : inputCoins) {
            txNew.vin.push_back(CTxIn(coin.outpoint, CScript(), nSequence));
        }

        CTxOut txout(totalValue - fee, newDestScriptPubKey);
        txNew.vout.push_back(txout);

        CTransaction txNewConst(txNew);
        int nIn = 0;
        for (const auto& coin : inputCoins) {
            const CScript& scriptPubKey = coin.txout.scriptPubKey;
            SignatureData sigdata;
            TransactionSignatureCreator signatureCreator(wallet.get(), &txNewConst, nIn, coin.txout.nValue, SIGHASH_ALL | SIGHASH_FORKID);

            BOOST_CHECK(ProduceSignature(signatureCreator, scriptPubKey, sigdata));
            UpdateTransaction(txNew, nIn, sigdata);

            nIn++;
        }

        CWalletTx wtxNew;
        wtxNew.fTimeReceivedIsTxTime = true;
        wtxNew.BindWallet(wallet.get());
        wtxNew.SetTx(MakeTransactionRef(std::move(txNew)));

        CReserveKey reservekey(wallet.get());
        CValidationState state;
        BOOST_CHECK(wallet->CommitTransaction(wtxNew, reservekey, nullptr, state));

        return MineBlockWithTx(wtxNew);
    }

    void RescanWalletTransactions()
    {
        WalletRescanReserver reserver(wallet.get());
        reserver.reserve();
        wallet->ScanForWalletTransactions(chainActive.Genesis(), nullptr, reserver);
    }

    std::unique_ptr<CWallet> wallet;

private:
    CWalletTx& MineBlockWithTx(const CWalletTx &wtx)
    {
        CMutableTransaction blocktx;
        {
            LOCK(wallet->cs_wallet);
            blocktx = CMutableTransaction(*wallet->mapWallet.at(wtx.GetHash()).tx);
        }

        CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        LOCK(wallet->cs_wallet);
        auto it = wallet->mapWallet.find(wtx.GetHash());
        BOOST_CHECK(it != wallet->mapWallet.end());
        it->second.SetMerkleBranch(chainActive.Tip(), 1);
        return it->second;
    }
};


BOOST_FIXTURE_TEST_SUITE(coinstake_tests, WalletTestingSetup)

CTxDestination CreateNewDestination(CWallet *const wallet, OutputType outputType)
{
    CPubKey newKey;
    BOOST_CHECK(wallet->GetKeyFromPool(newKey));
    wallet->LearnRelatedScripts(newKey, outputType);

    CTxDestination dest = GetDestinationForKey(newKey, outputType);
    return dest;
}

CScript GetExpectedOutputScriptPubKey(const CWallet *wallet, const COutPoint &prevOut)
{
    const CWalletTx *srcTx = wallet->GetWalletTx(prevOut.hash);
    CScript scriptPubKey = srcTx->tx->vout[prevOut.n].scriptPubKey;
    CScript result;
    txnouttype whichType;
    std::vector<std::vector<unsigned char>> vSolutions;

    BOOST_CHECK(Solver(scriptPubKey, whichType, vSolutions));

    if (whichType == TX_WITNESS_V0_SCRIPTHASH || whichType == TX_SCRIPTHASH) {
        uint160 hash;
        if(whichType == TX_SCRIPTHASH) {
            hash = uint160(vSolutions[0]);
        }
        else {
            CRIPEMD160().Write(&vSolutions[0][0], vSolutions[0].size()).Finalize(hash.begin());
        }

        CScriptID scriptID(hash);

        BOOST_CHECK(wallet->GetCScript(scriptID, scriptPubKey));
        BOOST_CHECK(Solver(scriptPubKey, whichType, vSolutions));
    }

    if (whichType == TX_PUBKEYHASH || whichType == TX_WITNESS_V0_KEYHASH) {
        // convert to pay to public key type
        // we need natural key for sign/verify PoS block
        CKey key;
        BOOST_CHECK(wallet->GetKey(CKeyID(uint160(vSolutions[0])), key));
        result << ToByteVector(key.GetPubKey()) << OP_CHECKSIG;
    }
    else if (whichType == TX_PUBKEY) {
        result = scriptPubKey;
    }
    else {
        BOOST_CHECK(false);
    }

    return result;
}

BOOST_FIXTURE_TEST_CASE(create_coinstake_tests, CoinStakeTestingSetup)
{
    int64_t nStartTime = GetTime();
    SetMockTime(nStartTime);

    const Consensus::Params& params = Params().GetConsensus();

    auto mineBlocks = [&](int count){
        for (int32_t i = 0; i < count; i++) {
            SetMockTime(GetTime() + 1000);
            CreateAndProcessBlock({}, GetScriptForRawPubKey(coinbaseKey.GetPubKey()));
        }
    };

    // Make mature all previoius 100 coinbases
    mineBlocks(100);

    // Transfer everything to a single distination of needed type
    CTxDestination destination = CreateNewDestination(wallet.get(), OutputType::OUTPUT_TYPE_P2SH_SEGWIT);
    CScript scriptPubKey = GetScriptForDestination(destination);
    CAmount balance = wallet->GetBalance();
    AddTx(CRecipient{scriptPubKey, balance, true});

    // Fulfill the Regtest params requirements for staking
    mineBlocks(1500);

    SetMockTime(GetTime() + 10 * 60);
    uint32_t coinStakeTime = GetTime();

    // Just check that we ready to stake
    std::vector<COutput> availableCoins;
    wallet->AvailableCoins(availableCoins, true, nullptr, coinStakeTime);
    BOOST_CHECK_EQUAL(availableCoins.size(), 1);

    // Test CoinStake transaction creation:
    uint64_t searchInterval = 60;
    uint32_t bits = UintToArith256(params.nInitialHashTargetPoS).GetCompact();
    CMutableTransaction coinStakeTx;
    CAmount posReward = 0;

    BOOST_CHECK(wallet->CreateCoinStake(*wallet.get(), bits, searchInterval, coinStakeTx, coinStakeTime, posReward));
    BOOST_CHECK_EQUAL(coinStakeTx.vin.size(), 1);
    BOOST_CHECK_EQUAL(coinStakeTx.vout.size(), 2); // output split is expected due to coinage

    // Ensure fees and amount are set correctly
    size_t txSize = ::GetSerializeSize(coinStakeTx, SER_NETWORK, PROTOCOL_VERSION);
    CFeeRate minFeeRate = CFeeRate(DEFAULT_TRANSACTION_MINFEE);
    CAmount fee = minFeeRate.GetFee(txSize);
    BOOST_CHECK_GE(posReward, fee);

    CAmount inputValue = 0;
    for (auto &in : coinStakeTx.vin) {
        const CWalletTx *srcTx = wallet->GetWalletTx(in.prevout.hash);
        inputValue += srcTx->tx->vout[in.prevout.n].nValue;
    }

    CAmount outputValue = 0;
    for (auto &out : coinStakeTx.vout) {
        outputValue += out.nValue;
    }

    BOOST_CHECK_EQUAL(outputValue + posReward, inputValue);

    CScript expectedScriptPubKey = GetExpectedOutputScriptPubKey(wallet.get(), coinStakeTx.vin[0].prevout);
    for (auto &out : coinStakeTx.vout) {
        BOOST_CHECK(out.scriptPubKey == expectedScriptPubKey);
    }

    // Test CoinStake Block Creation:
    bool posCancel = false;
    std::unique_ptr<CBlockTemplate> pblocktemplate = BlockAssembler(Params()).CreateNewPoSBlock(posCancel, wallet.get(), false);
    BOOST_CHECK(!posCancel);
    CBlock *block = &pblocktemplate->block;

    CBlockIndex* pindexPrev = chainActive.Tip();
    unsigned int nExtraNonce = 0;
    IncrementExtraNonce(block, pindexPrev, nExtraNonce);

    BOOST_CHECK(SignBlock(*block, *wallet.get()));

    std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*block);
    BOOST_CHECK(ProcessNewBlock(Params(), shared_pblock, true, nullptr));

    // Ensure that we can spend outputs of a coinstake block:
    {
        mineBlocks(100);

        RescanWalletTransactions();

        // Prepare inputs for transfer
        std::set<CInputCoin> inputCoins;
        CAmount totalValue = 0;
        for (const auto &txRef : block->vtx) {
            const CWalletTx *wtx = wallet->GetWalletTx(txRef->GetHash());

            unsigned int i = 0;
            for (auto &vout : txRef->vout) {
                if (vout.nValue > 0) {
                    inputCoins.insert(CInputCoin(wtx, i));
                    totalValue += vout.nValue;
                }

                ++i;
            }
        }

        // Get any new destination
        CTxDestination newDest = CreateNewDestination(wallet.get(), OutputType::OUTPUT_TYPE_P2SH_SEGWIT);

        // Act
        const CWalletTx &newWtx = Transfer(inputCoins, newDest);

        // Assert: check the coins are avaiable now on the new destination
        wallet->AvailableCoins(availableCoins, true, nullptr, GetTime());
        auto iterator = std::find_if(availableCoins.begin(), availableCoins.end(),
            [&newWtx](const COutput &out) { return out.tx->GetHash() == newWtx.GetHash() && out.i == 0; });

        BOOST_CHECK(iterator != availableCoins.end());
    }
}

BOOST_AUTO_TEST_SUITE_END()
