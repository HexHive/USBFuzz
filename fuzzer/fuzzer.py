import os
import sys
import subprocess

class USBFuzz(object):

    def __init__(self, workdir, seeddir, aflfuzz_path, qemu_path,
               os_image, kernel_image=None, afl_opts=None):
        self.os_image = os_image
        self.kernel_image = kernel_image
        self.workdir = workdir
        self.seeddir = seeddir
        self.aflfuzz_path = aflfuzz_path
        self.qemu_path = qemu_path
        self.afl_opts = afl_opts
        self.args = []

        self._setup_fuzzer()

        self.process = None

    def _setup_fuzzer(self):
        self.args.append(self.aflfuzz_path)
        self.args.append("-QQ")
        if self.afl_opts != None:
            self.args.extend(self.afl_opts.split(" "))

        self.args.append("-i")
        self.args.append(self.seeddir)
        self.args.append("-o")
        self.args.append(self.workdir)
        self.args.append("--")
        self.args.append(self.qemu_path)
        self.args.extend("-M q35 -snapshot -device qemu-xhci,id=xhci -m 4G -enable-kvm".split(" "))
        self.args.extend("-object memory-backend-shm,id=shm -device ivshmem-plain,id=ivshmem,memdev=shm".split(" "))

        if self.kernel_image != None:
            # this is a linux system
            self.args.append("-kernel")
            self.args.append(self.kernel_image)
            self.args.append("-append")
            self.args.append("root=/dev/sda")

        self.args.append("-hda")
        self.args.append(self.os_image)
        self.args.extend("-no-reboot -nographic -usbDescFile @@".split(" "))

    def start(self):
        # setting up some env variables
        if len(self.args) == 0:
            print("fuzzer not setup")
            return
        env = os.environ.copy()
        env["AFL_NO_UI"] = "1"
        env["AFL_NO_ARITH"] = "1"
        env["AFL_FAST_CAL"] = "1"
        env["AFL_SKIP_CPUFREQ"] = "1"
        self.process = subprocess.Popen(self.args, env=env)

    def stop(self):
        if self.process != None:
            self.process.kill()
