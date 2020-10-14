#!/bin/sh


cd ivshmem_driver; make; cd ..
cd uma-src; make ; cd ..

if [ -e /bin/uma-files ]; then
    rm -rf /bin/uma-files
fi

mkdir -p /bin/uma-files
cp ivshmem_driver/ivshmem.ko /bin/uma-files
cp uma-src/uma /bin/uma-files

cp uma.sh /bin

cp rc-script/uma /etc/rc.d
