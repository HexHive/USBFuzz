#!/bin/sh

device="/dev/ivshmem"
module_name="kvm_ivshmem"

if [ ! -e $device ]; then
    major=`cat /proc/devices | grep $module_name | awk '{print $1}'`
    mknod --mode=666 $device c $major 0
else
    chmod 666 $device
fi
