Bitcoin Atom [BCA] integration/staging tree
============================================

<p align="center">
<img src="https://user-images.githubusercontent.com/34191200/35906968-0a422cb8-0c1f-11e8-85e4-7be0ccb6019d.png" />
</p>

What is Bitcoin Atom (BCA) ?
----------------

Bitcoin Atom (BCA) is a SegWit enabled Bitcoin fork with on-chain atomic swaps on board and hybrid consensus. Bitcoin Atom enables truly decentralized digital asset exchange by utilizing hash time-locked contracts (HTLCs) and its own HTLC API, giving independence from intermediaries and any centralized entities.

Bitcoin Atom is a fork of the Bitcoin blockchain with major protocol upgrades that occurred in January 2018. The Atom development team is working on a special embedded toolkit for both on-chain and off-chain atomic swaps, allowing for exchanging any cryptocurrencies in a hassle-free way across different blockchains.

The Bitcoin Atom (BCA) fork took place at block #505,888 on January 24, 2018.

BCA codebase is forked from Bitcoin Core which is a Bitcoin full node implementation written in C++. Bitcoin Core is a ongoing project under active development. As Bitcoin Atom is constantly synced with the Bitcoin Core codebase, it will get the benefit of Core's ongoing upgrades to sidechain activations, peer and connection handling, database optimizations and other blockchain related technology improvements.

Atom node (atomd) acts as a chain daemon for the BCA cryptocurrency. atomd maintains the entire transactional ledger of Bitcoin Atom and allows relaying of transactions to other BCA nodes around the world. Please [see our wiki](https://github.com/bitcoin-atom/bitcoin-atom/wiki) for more info on technical details.

Note: Bitcoin Atom supports a hybrid consensus approach, allowing BCA holders to earn on transaction fees via Proof-of-Stake (PoS) block minting. PoS block generation is enabled by default and supported by the node at its core. 

This project is currently under active development and is in a Beta state.

For additional information, please visit Bitcoin Atom’s website at https://bitcoinatom.io

Downloads
-------

Bitcoin Atom node for your OS (Windows, Mac, Linux): https://github.com/bitcoin-atom/bitcoin-atom/releases

Bitcoin Atom source code: https://github.com/bitcoin-atom/bitcoin-atom

Build Requirements
-------

The following dependencies are required:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 libssl      | Crypto           | Random Number Generation, Elliptic Curve Cryptography
 libboost    | Utility          | Library for threading, data structures, etc
 libevent    | Networking       | OS independent asynchronous networking

Optional dependencies:

 Library     | Purpose          | Description
 ------------|------------------|----------------------
 miniupnpc   | UPnP Support     | Firewall-jumping support
 libdb4.8    | Berkeley DB      | Wallet storage (only needed when wallet enabled)
 qt          | GUI              | GUI toolkit (only needed when GUI enabled)
 protobuf    | Payments in GUI  | Data interchange format used for payment protocol (only needed when GUI enabled)
 libqrencode | QR codes in GUI  | Optional for generating QR codes (only needed when GUI enabled)
 univalue    | Utility          | JSON parsing and encoding (bundled version will be used unless --with-system-univalue passed to configure)
 libzmq3     | ZMQ notification | Optional, allows generating ZMQ notifications (requires ZMQ version >= 4.x)

For the versions used, see [dependencies.md](https://github.com/bitcoin-atom/bitcoin-atom/blob/master/doc/dependencies.md)

How to Build
-------
```bash
./autogen.sh
./configure
make
make install # optional
```

This will build atom-qt as well if the dependencies are met.

Issue Tracker
-------
The integrated GitHub issue tracker is used for this project. Upon running into an issue, please submit it [here](https://github.com/bitcoin-atom/bitcoin-atom/issues).

Documentation
-------
The documentation is a work-in-progress. It is located in the doc folder and [the wiki pages](https://github.com/bitcoin-atom/bitcoin-atom/wiki).

Wiki
-------
The answers to most technical questions can be found in the official BCA wiki:
https://github.com/bitcoin-atom/bitcoin-atom/wiki

This wiki will be updated with BCA specifications, docs, manuals and FAQs.

License
-------

Bitcoin Atom [BCA] is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.
