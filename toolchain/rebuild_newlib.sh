#!/bin/bash

. ./config.sh

if [[ ! -d build ]]; then
	mkdir build || exit
fi

cd build

if [[ CLEANFIRST && -d newlib-${NEWLIB_VER} ]]; then
	if [[ -d newlib-${NEWLIB_VER} ]]; then
		rm -rf newlib-${NEWLIB_VER}
	fi
fi

if [[ ! -d newlib-${NEWLIB_VER} ]]; then
	wget -c ftp://sourceware.org/pub/newlib/newlib-${NEWLIB_VER}.tar.gz || exit
	tar xzf newlib-${NEWLIB_VER}.tar.gz || exit
	patch -p0 < ../newlib.patch || exit
fi

rm -rf newlib-${NEWLIB_VER}/newlib/libc/sys/dionysus || exit
cp -r ../newlib-files newlib-${NEWLIB_VER}/newlib/libc/sys/dionysus || exit

pushd newlib-${NEWLIB_VER}/newlib/libc/sys
	WANT_AUTOCONF=2.68 autoconf || exit
	pushd dionysus
		WANT_AUTOCONF=2.68 WANT_AUTOMAKE=1.11 autoreconf || exit
	popd
popd

rm -rf build-newlib || exit
mkdir build-newlib || exit

pushd build-newlib
	../newlib-${NEWLIB_VER}/configure --target=$TARGET --prefix=$VPREFIX || exit
	make || exit
	make DESTDIR=$SYSROOT install
	make DESTDIR=$SYSROOT tooldir=$VPREFIX install
popd