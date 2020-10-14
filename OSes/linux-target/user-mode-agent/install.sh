#!/bin/sh

cd uma; make; cd ..

if [ ! -d /bin/uma/ ]; then
    mkdir /bin/uma
fi

cp uma/uma /bin/uma/


cp uma-systemd/prepare-devfile.sh /bin/uma
cp uma-systemd/uma-env /bin/uma
chmod +x /bin/uma/prepare-devfile.sh
cp uma-systemd/uma.service /etc/systemd/system/

chmod 644 /etc/systemd/system/uma.service

systemctl enable uma
