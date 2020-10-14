#!/bin/bash

QEMU=<PATH_TO_USBFUZZ>/usbfuzz-afl/qemu_mode/qemu-macos/x86_64-softmmu/qemu-system-x86_64
ALF_FUZZ=<PATH_TO_USBFUZZ>/usbfuzz-afl/afl-fuzz
IMAGE_PATH=<IMAGE_PATH>

export AFL_ENABLE_TARGET_OUTPUT=1
export AFL_QEMU_TARGET_WAIT=1000
export AFL_QEMU_TEST_WAIT=5000

$AFL_FUZZ -n -QQ -i $1 -o $2 -- \
			     ${QEMU} -enable-kvm -m 4G \
			     -machine q35 \
			     -device qemu-xhci,id=xhci \
			     -hda $IMAGE_PATH \
			     -nographic \
			     -no-reboot \
			     -nographic \
			     -usbDescFile @@

