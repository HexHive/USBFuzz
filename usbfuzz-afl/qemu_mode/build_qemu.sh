#!/bin/bash

_cwd=`pwd`

QEMU_BUILD_DIR=qemu-build

rm -rf $QEMU_BUILD_DIR
mkdir $QEMU_BUILD_DIR
cd $QEMU_BUILD_DIR

../qemu/configure --target-list="x86_64-softmmu"
make
