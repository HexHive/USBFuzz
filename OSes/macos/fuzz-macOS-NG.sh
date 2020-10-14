#!/bin/bash

# qemu-img create -f qcow2 mac_hdd_ng.img 128G
#
# echo 1 > /sys/module/kvm/parameters/ignore_msrs (this is required)

############################################################################
# NOTE: Tweak the "MY_OPTIONS" line in case you are having booting problems!
############################################################################

# This works for High Sierra as well as Mojave. Tested with macOS 10.13.6 and macOS 10.14.4.

MY_OPTIONS="+pcid,+ssse3,+sse4.2,+popcnt,+avx,+aes,+xsave,+xsaveopt,check"

# OVMF=./firmware
OVMF="./"


# QEMU=qemu-system-x86_64
# QEMU=$HOME/src/qemu-build/x86_64-softmmu/qemu-system-x86_64
QEMU=<PATH_TO_USBFUZZ>/usbfuzz-afl/qemu_mode/qemu-build/x86_64-softmmu/qemu-system-x86_64
AFL_FUZZ=<PATH_TO_USB_FUZZ>/usbfuzz-afl/afl-fuzz

export AFL_ENABLE_TARGET_OUTPUT=1
export AFL_QEMU_TARGET_WAIT=50000
export AFL_QEMU_TEST_WAIT=3000

$AFL_FUZZ -n -QQ -i $1 -o $2 -- \
    ${QEMU} -enable-kvm -m 3G -cpu Penryn,kvm=on,vendor=GenuineIntel,+invtsc,vmware-cpuid-freq=on,$MY_OPTIONS\
    -machine q35 \
    -smp 4,cores=2 \
    -usb -device usb-kbd -device usb-tablet \
    -device isa-applesmc,osk="ourhardworkbythesewordsguardedpleasedontsteal(c)AppleComputerInc" \
    -drive if=pflash,format=raw,readonly,file=$OVMF/OVMF_CODE.fd \
    -drive if=pflash,format=raw,readonly,file=$OVMF/OVMF_VARS-1024x768.fd \
    -smbios type=2 \
    -device ich9-intel-hda -device hda-duplex \
    -device ich9-ahci,id=sata \
    -drive id=Clover,if=none,snapshot=on,format=qcow2,file=./'Mojave/CloverNG.qcow2' \
    -device ide-hd,bus=sata.2,drive=Clover \
    -device ide-hd,bus=sata.3,drive=InstallMedia \
    -drive id=InstallMedia,if=none,file=BaseSystem.qcow2,format=qcow2 \
    -drive id=MacHDD,if=none,file=./mac_hdd_ng.img,format=qcow2 \
    -device ide-hd,bus=sata.4,drive=MacHDD \
    -netdev tap,id=net0,ifname=tap0,script=no,downscript=no -device vmxnet3,netdev=net0,id=net0,mac=52:54:00:c9:18:27 \
    -vga vmware \
    -no-reboot \
    -no-graphic \
    -usbDescFile @@
