#!/bin/sh

res=`ps aux | grep uma`
if [ "$res" != "" ]; then
    killall uma
fi

daemon -o /var/log/uma.log /bin/uma-files/uma
