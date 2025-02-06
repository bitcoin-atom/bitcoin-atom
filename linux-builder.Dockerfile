FROM ubuntu:14.04

RUN apt-get update && apt-get install -y \
    curl \
    g++-aarch64-linux-gnu \
    g++-4.8-aarch64-linux-gnu \
    gcc-4.8-aarch64-linux-gnu \
    binutils-aarch64-linux-gnu \
    g++-arm-linux-gnueabihf \
    g++-4.8-arm-linux-gnueabihf \
    gcc-4.8-arm-linux-gnueabihf \
    binutils-arm-linux-gnueabihf \
    g++-4.8-multilib \
    gcc-4.8-multilib \
    binutils-gold \
    git-core \
    pkg-config \
    autoconf \
    libtool \
    automake \
    faketime \
    bsdmainutils \
    ca-certificates \
    python \
    make \
    build-essential

WORKDIR /build

COPY . bitcoin-atom
COPY ./build-linux.sh .

# RUN make -C depends download SOURCES_PATH=`pwd`/cache/common

RUN chmod +x /build/build-linux.sh

CMD ["/build/build-linux.sh"]
