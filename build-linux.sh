#!/bin/bash

set -e

make -C bitcoin-atom/depends download SOURCES_PATH=`pwd`/cache/common

WRAP_DIR=$HOME/wrapped
HOSTS="i686-pc-linux-gnu x86_64-linux-gnu arm-linux-gnueabihf aarch64-linux-gnu"
CONFIGFLAGS="--enable-glibc-back-compat --enable-reduce-exports --disable-bench --disable-gui-tests --disable-tests"
FAKETIME_HOST_PROGS=""
FAKETIME_PROGS="date ar ranlib nm"
HOST_CFLAGS="-O2 -g"
HOST_CXXFLAGS="-O2 -g"
HOST_LDFLAGS=-static-libstdc++
OUTDIR=/build/output
MAKEOPTS="-j8"

export QT_RCC_TEST=1
export GZIP="-9n"
export TAR_OPTIONS="--mtime="2000-01-01\\\ 12:00:00""
export TZ="UTC"
export BUILD_DIR=`pwd`
mkdir -p ${WRAP_DIR}

function create_global_faketime_wrappers {
for prog in ${FAKETIME_PROGS}; do
  echo '#!/bin/bash' > ${WRAP_DIR}/${prog}
  echo "REAL=\`which -a ${prog} | grep -v ${WRAP_DIR}/${prog} | head -1\`" >> ${WRAP_DIR}/${prog}
  echo 'export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/faketime/libfaketime.so.1' >> ${WRAP_DIR}/${prog}
  echo "export FAKETIME=\"$1\"" >> ${WRAP_DIR}/${prog}
  echo "\$REAL \$@" >> $WRAP_DIR/${prog}
  chmod +x ${WRAP_DIR}/${prog}
done
}

function create_per-host_faketime_wrappers {
for i in $HOSTS; do
  for prog in ${FAKETIME_HOST_PROGS}; do
      echo '#!/bin/bash' > ${WRAP_DIR}/${i}-${prog}
      echo "REAL=\`which -a ${i}-${prog} | grep -v ${WRAP_DIR}/${i}-${prog} | head -1\`" >> ${WRAP_DIR}/${i}-${prog}
      echo 'export LD_PRELOAD=/usr/lib/x86_64-linux-gnu/faketime/libfaketime.so.1' >> ${WRAP_DIR}/${i}-${prog}
      echo "export FAKETIME=\"$1\"" >> ${WRAP_DIR}/${i}-${prog}
      echo "\$REAL \$@" >> $WRAP_DIR/${i}-${prog}
      chmod +x ${WRAP_DIR}/${i}-${prog}
  done
done
}

# Faketime for depends so intermediate results are comparable
export PATH_orig=${PATH}
create_global_faketime_wrappers "2000-01-01 12:00:00"
create_per-host_faketime_wrappers "2000-01-01 12:00:00"
export PATH=${WRAP_DIR}:${PATH}

EXTRA_INCLUDES_BASE=$WRAP_DIR/extra_includes
mkdir -p $EXTRA_INCLUDES_BASE

# x86 needs /usr/include/i386-linux-gnu/asm pointed to /usr/include/x86_64-linux-gnu/asm,
# but we can't write there. Instead, create a link here and force it to be included in the
# search paths by wrapping gcc/g++.

mkdir -p $EXTRA_INCLUDES_BASE/i686-pc-linux-gnu
rm -f $WRAP_DIR/extra_includes/i686-pc-linux-gnu/asm
ln -s /usr/include/x86_64-linux-gnu/asm $EXTRA_INCLUDES_BASE/i686-pc-linux-gnu/asm

for prog in gcc g++; do
rm -f ${WRAP_DIR}/${prog}
cat << EOF > ${WRAP_DIR}/${prog}
#!/bin/bash
REAL="`which -a ${prog} | grep -v ${WRAP_DIR}/${prog} | head -1`"
for var in "\$@"
do
  if [ "\$var" = "-m32" ]; then
    export C_INCLUDE_PATH="$EXTRA_INCLUDES_BASE/i686-pc-linux-gnu"
    export CPLUS_INCLUDE_PATH="$EXTRA_INCLUDES_BASE/i686-pc-linux-gnu"
    break
  fi
done
\$REAL \$@
EOF
chmod +x ${WRAP_DIR}/${prog}
done

cd bitcoin-atom
BASEPREFIX=`pwd`/depends
# Build dependencies for each host
for i in $HOSTS; do
  EXTRA_INCLUDES="$EXTRA_INCLUDES_BASE/$i"
  if [ -d "$EXTRA_INCLUDES" ]; then
    export HOST_ID_SALT="$EXTRA_INCLUDES"
  fi
  make ${MAKEOPTS} -C ${BASEPREFIX} HOST="${i}"
  unset HOST_ID_SALT
done

# Faketime for binaries
export PATH=${PATH_orig}
create_global_faketime_wrappers "2000-01-01 12:00:00"
create_per-host_faketime_wrappers "2000-01-01 12:00:00"
export PATH=${WRAP_DIR}:${PATH}

# Create the release tarball using (arbitrarily) the first host
./autogen.sh
CONFIG_SITE=${BASEPREFIX}/`echo "${HOSTS}" | awk '{print $1;}'`/share/config.site ./configure --prefix=/
make dist
SOURCEDIST=`echo atom-*.tar.gz`
DISTNAME=`echo ${SOURCEDIST} | sed 's/.tar.*//'`
# Correct tar file order
mkdir -p temp
pushd temp
tar xf ../$SOURCEDIST
find atom-* | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ../$SOURCEDIST
popd

# Workaround for tarball not building with the bare tag version (prep)
make -C src obj/build.h

ORIGPATH="$PATH"
# Extract the release tarball into a dir for each host and build
for i in ${HOSTS}; do
  export PATH=${BASEPREFIX}/${i}/native/bin:${ORIGPATH}
  mkdir -p distsrc-${i}
  cd distsrc-${i}
  INSTALLPATH=`pwd`/installed/${DISTNAME}
  mkdir -p ${INSTALLPATH}
  tar --strip-components=1 -xf ../$SOURCEDIST

  # Workaround for tarball not building with the bare tag version
  echo '#!/bin/true' >share/genbuild.sh
  mkdir src/obj
  cp ../src/obj/build.h src/obj/

  CONFIG_SITE=${BASEPREFIX}/${i}/share/config.site ./configure --prefix=/ --disable-ccache --disable-maintainer-mode --disable-dependency-tracking ${CONFIGFLAGS} CFLAGS="${HOST_CFLAGS}" CXXFLAGS="${HOST_CXXFLAGS}" LDFLAGS="${HOST_LDFLAGS}"
  make ${MAKEOPTS}
  make ${MAKEOPTS} -C src check-security

  #TODO: This is a quick hack that disables symbol checking for arm.
  #      Instead, we should investigate why these are popping up.
  #      For aarch64, we'll need to bump up the min GLIBC version, as the abi
  #      support wasn't introduced until 2.17.
  case $i in
     aarch64-*) : ;;
     arm-*) : ;;
     *) make ${MAKEOPTS} -C src check-symbols ;;
  esac

  make install DESTDIR=${INSTALLPATH}
  cd installed
  find . -name "lib*.la" -delete
  find . -name "lib*.a" -delete
  rm -rf ${DISTNAME}/lib/pkgconfig
  find ${DISTNAME}/bin -type f -executable -exec ../contrib/devtools/split-debug.sh {} {} {}.dbg \;
  find ${DISTNAME}/lib -type f -exec ../contrib/devtools/split-debug.sh {} {} {}.dbg \;
  find ${DISTNAME} -not -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}.tar.gz
  find ${DISTNAME} -name "*.dbg" | sort | tar --no-recursion --mode='u+rw,go+r-w,a+X' --owner=0 --group=0 -c -T - | gzip -9n > ${OUTDIR}/${DISTNAME}-${i}-debug.tar.gz
  cd ../../
  rm -rf distsrc-${i}
done
mkdir -p $OUTDIR/src
mv $SOURCEDIST $OUTDIR/src
