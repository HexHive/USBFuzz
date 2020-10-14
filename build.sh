#!/bin/bash

cd usbfuzz-afl
make

cd qemu_mode
./build_qemu.sh
cd ../..
