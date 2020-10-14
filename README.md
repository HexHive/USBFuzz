# Environment

The setup is tested on Ubuntu 18.04.5 LTS.

# prerequisites

Please uncomment `deb-src` lines in `/etc/apt/sources.list` in your system,
then run the following commands:

```
sudo apt-get update
sudo apt-get build-dep qemu
```

Then please run `build.sh` to build the fuzzer and qemu.

# Fuzzing linux kernel

## Preparing the kernel

1. download a version of Linxu kernel and apply the patches in `OSes/linux-target/kernel-patches` (the patches are based on v5.5, if you use other versions, the patches may not apply directly and need to be modified). 
2. start building the kernel using the kernel config file in `OSes/linux-target/kconfig`

## Preparing a Linux userspace image

If you do not want to build the image by yourself, you can download an image directly from [here](https://hexdump.cs.purdue.edu/usbfuzz/stretch.img).

Otherwise, run the script in `scripts` directory using the following command:

```
scripts/create-image.sh -f full
```

After the system image is built, please run the image using the following image

```
usbfuzz-afl/qemu_mode/qemu-build/x86_64-softmmu/qemu-system-x86_64 -M q35 -net nic,model=e1000 -net user,host=10.0.2.10,hostfwd=tcp::1569-:22 -m 4G -enable-kvm -object memory-backend-shm,id=shm -device ivshmem-plain,id=ivshmem,memdev=shm -kernel <path_to_bzImage> -append "root=/dev/sda console=ttyS0" -hda <path_to_linux_image_file>
```

After the guest system is up, copy `OSes/linux-target/user-mode-agent` to the guest system and 
run `install.sh` in the guest system.

## running the fuzzer

Please use the python frontend named `USBFuzz` to start the fuzzer (for how to use the script,
please run `USBFuzz --help`). Before starting the fuzzer, you may need to run the `scripts/kernel_config.sh`.

E.g.: 

```
/USBFuzz --seeddir seeds --kernel_image OSes/linux-target/linux-test-build/arch/x86_64/boot/bzImage --os_image images/linux/stretch.img 
```

## reproducing a bug

Given that you have found a USB bug, you may need to reproduce the bug.
Assume that we have found a bug and the crashing input is saved in `bugs/bug1`.

1. run the following command to start the guest vm: 
```
./usbfuzz-afl/qemu_mode/qemu-build/x86_64-softmmu/qemu-system-x86_64 -M q35 -device qemu-xhci,id=xhci  -object memory-backend-shm,id=shm -device ivshmem-plain,id=ivshmem,memdev=shm  -m 4G -enable-kvm -kernel OSes/linux-target/linux-test-build/arch/x86_64/boot/bzImage  -hda images/linux/stretch.img -append 'root=/dev/sda console=ttyS0' -usbDescFile seeds/usb_sk4Wm9j -serial stdio
```

2. After the VM is startup, in the `View` menu, click `Show Tabs`, then in the `compat_monitor0` tab, input the following command, 

```
device_add usb-fuzz,id=fuzz1
```

Then you will get a bug report on the terminal.

# Fuzzing FreeBSD

To fuzz FreeBSD system, you can download a FreeBSD system image from [here](https://hexdump.cs.purdue.edu/usbfuzz/FreeBSD12_64.qcow2)
To start fuzzing, run the following command:

```
./USBFuzz  --aflfuzz_opts "-n" --seeddir seeds --os_image <path_to_freebsd_image>
```
