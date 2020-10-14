# Fuzzing windows

To fuzz windows, first a windows image is needed.
Please download a windows image for virtualbox from
[here](https://developer.microsoft.com/en-us/microsoft-edge/tools/vms/)
and then convert the image to qcow2 format(see
[here](https://computingforgeeks.com/how-to-convert-virtualbox-disk-image-vdi-and-img-to-qcow2-format/)).


Then update the following variables in `fuzz-windows.sh`.

* QEMU
* ALF_FUZZ
* IMAGE_PATH


To start fuzzing, just pass the seed directory and work directory as argument to `fuzz-windws.sh`