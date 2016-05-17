#!/bin/bash

export BINUTILS_VER=2.26
export GCC_VER=6.1.0
export NEWLIB_VER=2.4.0

export CLEANFIRST=true
export BUILD_TARGET=false

export TARGET=i686-dionysus
export PREFIX=/usr/$TARGET
export VPREFIX=/usr
export SYSROOT=`readlink -f $(pwd)/../hdd`
export PATH=$PATH:$PREFIX/bin