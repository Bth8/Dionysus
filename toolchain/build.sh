#!/bin/bash

. ./config.sh

if [[ ! -d build ]]; then
	mkdir build
fi
cd build

# Cleanup
if [[ CLEANFIRST ]]; then
	echo "Cleaning up"
	if [[ -d binutils-${BINUTILS_VER} ]]; then
		rm -rf binutils-${BINUTILS_VER}
	fi
	if [[ -d build-binutils ]]; then
		rm -rf build-binutils
	fi

	if [[ -d gcc-${GCC_VER} ]]; then
		rm -rf gcc-${GCC_VER}
	fi
	if [[ -d build-gcc ]]; then
		rm -rf build-gcc
	fi

	if [[ -d newlib-${NEWLIB_VER} ]]; then
		rm -rf newlib-${NEWLIB_VER}
	fi
	if [[ -d build-newlib ]]; then
		rm -rf build-newlib
	fi
fi

# Start Fetching
echo "Fetch binutils"
wget -c http://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VER}.tar.gz
tar xzf binutils-${BINUTILS_VER}.tar.gz

echo "Fetch GCC"
wget -c http://ftp.gnu.org/gnu/gcc/gcc-${GCC_VER}/gcc-${GCC_VER}.tar.gz
tar xzf gcc-${GCC_VER}.tar.gz

echo "Fetch newlib"
wget -c ftp://sourceware.org/pub/newlib/newlib-${NEWLIB_VER}.tar.gz
tar xzf newlib-${NEWLIB_VER}.tar.gz

# Patch
echo "Patch binutils"
patch -p0 < ../binutils.patch || exit

echo "Patch GCC"
patch -p0 < ../gcc.patch || exit

echo "Patch newlib"
patch -p0 < ../newlib.patch || exit
cp -r ../newlib-files newlib-${NEWLIB_VER}/newlib/libc/sys/dionysus || exit

# Fix shit (autoconf, etc.)
echo "Fix binutils"
pushd binutils-${BINUTILS_VER}/ld
	WANT_AUTOCONF=2.64 aclocal || exit
	WANT_AUTOCONF=2.64 automake || exit
popd

echo "Fix gcc"
pushd gcc-${GCC_VER}/libstdc++-v3
	WANT_AUTOCONF=2.64 autoconf || exit
popd

echo "Fix newlib"
pushd newlib-${NEWLIB_VER}/newlib/libc/sys
	WANT_AUTOCONF=2.68 autoconf || exit
	pushd dionysus
		WANT_AUTOCONF=2.68 WANT_AUTOMAKE=1.11 autoreconf || exit
	popd
popd

# Compile
echo "Make build dirs"
mkdir build-binutils
mkdir build-gcc
mkdir build-newlib

echo "Compile binutils"
pushd build-binutils
	../binutils-${BINUTILS_VER}/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT || exit
	make || exit
	if [[ ! -G $(dirname $PREFIX) && $EUID != 0 ]]; then
		sudo make install || exit
	else
		make install || exit
	fi
popd

echo "Compile GCC"
pushd build-gcc
	../gcc-${GCC_VER}/configure --target=$TARGET --prefix=$PREFIX --with-sysroot=$SYSROOT --disable-nls --enable-languages=c,c++ --with-newlib || exit
	make all-gcc || exit
	make all-target-libgcc || exit
	if [[ ! -G $(dirname $PREFIX) && $EUID != 0 ]]; then
		sudo make install-gcc || exit
		sudo make install-target-libgcc || exit
	else
		make install-gcc || exit
		make install-target-libgcc || exit
	fi
popd

echo "Compile newlib"
pushd build-newlib
	../newlib-${NEWLIB_VER}/configure --target=$TARGET --prefix=$VPREFIX || exit
	make || exit
	make DESTDIR=$SYSROOT install || exit
	make DESTDIR=$SYSROOT tooldir=$VPREFIX install || exit
popd

echo "Compile GCC pass 2"
pushd build-gcc
	make || exit
	if [[ ! -G $(dirname $PREFIX) && $EUID != 0 ]]; then
		sudo make install || exit
	else
		make install || exit
	fi
popd

if [[ BUILD_TARGET ]]; then
	echo "Target binutils"
	pushd build-binutils
		../binutils-${BINUTILS_VER}/configure --host=$TARGET --target=$TARGET --prefix=$VPREFIX || exit
		make || exit
		make DESTDIR=$SYSROOT install || exit
	popd

	echo "Target GCC"
	pushd build-gcc
		../gcc-${GCC_VER}/configure --host=$TARGET --target=$TARGET --prefix=$VPREFIX --disable-nls --enable-languages=c,c++ --with-newlib || exit
		make || exit
		make DESTDIR=$SYSROOT install || exit
	popd
fi