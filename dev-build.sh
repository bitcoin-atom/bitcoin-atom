#!/bin/bash

./contrib/install_db4.sh `pwd`
./autogen.sh

export BDB_PREFIX='/src/db4'
./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include" --without-gui --without-miniupnpc --disable-tests --disable-shared

make -j8
