# Fuzzing MacOS

First you need to prepare a MacOS system that can be run in QEMU.
we use the method shown in [this repo](https://github.com/kholia/OSX-KVM.git).
Please first setup a guest system following the instructions in this repo first.

Then run `setup-network-intf.sh` to setup network interfaces for the guest.

Setup  the `QEMU` and `AFL_FUZZ` variables in `fuzz-macOS-NG.sh` . 

And use `fuzz-macOS-NG.sh` to start fuzzing, by passing the seed directory and work directory.

```
fuzz-macOS-NG.sh seeds workdir
```

