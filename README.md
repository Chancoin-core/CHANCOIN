Download Windows binaries [here](https://github.com/Chancoin-core/CHANCOIN/releases/latest) 
===

ChanCoin integration/staging tree
================================

http://www.chancoin.org

Copyright (c) 2009-2014 Bitcoin Developers
Copyright (c) 2011-2014 ChanCoin Developers

What is ChanCoin?
----------------

ChanCoin is a lite version of Bitcoin using scrypt as a proof-of-work algorithm.
 - 5 minute block targets
 - subsidy halves in 210k blocks (~2 years)
 - ~21 million total mined coins
 - 2 blocks to retarget difficulty
 - ~3.8 million premine (15%) for memes, airdrops, tipping, infrastructure, and dev purposes


The rest is the same as Bitcoin.
 - 50 coins per block

For more information, as well as an immediately useable, binary version of
the ChanCoin client sofware, see http://www.chancoin.org.

Installation
------------

If you're on Windows, the installation is simple -- just download the binaries from https://github.com/Chancoin-core/CHANCOIN/releases/latest. 

If you're on unix, since at the moment we don't have Linux/OSX binaries, you'll have to
compile yourself. It is a relatively easy process, however, and is explained in greater detail [here](https://github.com/Chancoin-core/CHANCOIN/blob/master/doc/readme-qt.rst). If you just want a quick tutorial, it is quite simple:

First, install the build dependencies:
```
sudo apt-get install qt4-qmake libqt4-dev build-essential libboost-dev libboost-system-dev \
    libboost-filesystem-dev libboost-program-options-dev libboost-thread-dev \
    libssl-dev libdb++-dev libminiupnpc-dev
```

Then, the qmake dependencies:

`sudo apt-get install qt5-qmake libqt5gui5 libqt5core5a libqt5dbus5 qttools5-dev-tools`


Now clone the repository:

`git clone https://github.com/Chancoin-core/CHANCOIN chancoin`

Then `cd` into chancoin and run `qmake` and `make`. 

License
-------

ChanCoin is released under the terms of the MIT license. See `COPYING` for more
information or see http://opensource.org/licenses/MIT.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the ChanCoin
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion with the devs and community.

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see `doc/coding.txt`) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/chancoin-project/chancoin/tags) are created
regularly to indicate new official, stable release versions of ChanCoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test. Please be patient and help out, and
remember this is a security-critical project where any mistake might cost people
lots of money.

### Automated Testing

Developers are strongly encouraged to write unit tests for new code, and to
submit new unit tests for old code.

Unit tests for the core code are in `src/test/`. To compile and run them:

	cd src; make -f makefile.unix test

Unit tests for the GUI code are in `src/qt/test/`. To compile and run them:

    qmake BITCOIN_QT_TEST=1 -o Makefile.test bitcoin-qt.pro
    make -f Makefile.test
    ./chancoin-qt_test

