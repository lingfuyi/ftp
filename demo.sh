#!/bin/sh
cd /data
sleep 20
while :
do
updatefile=/data/update/tandaUpdate.tar.gz
md5=/data/update/tandaUpdate.tar.gz.md5sum
    if test -f "$updatefile"
    then
        if test -f "$md5"
        then
        . update.sh
       wait 
       exit
        fi
    fi
#check net
    /etc/ipChange &

#check web config and restart SteamSensor
    tmp=`cat uplog`
    if [ $tmp -eq 6 ]
    then
        echo "read 6"
        pkill -9 SteamSensor
        echo 5 > uplog
    fi

#check udhcpc
    num=`ps | grep  'udhcpc -i eth0:1'  | grep -v grep | wc -l`
    if [ $num -gt 3 ]
    then
       echo "kill udhcpc"
       killall udhcpc
    fi


    RunningTask=$(ps | grep "SteamSensor" |grep -v "grep")
    if [ $? -ne 0 ]
    then
        File=$(date "+/data/%Y\%m\%d.thingworx.log")
        if [ ! -f "$File" ];then
            touch "$File"
        fi
        echo "Current DIR is " $PWD
        echo "Service not running......"
        echo "Starting service......"
        /data/SteamSensor >> $File &
    else
        echo " ]CküService was already started"
        echo "runing......"
    fi

    FileSize=$(ls -l $File | awk '{print $5}')
    if [ $FileSize -gt $((10000*1024)) ]
    then
        echo > $File
    fi

    sleep 4
done
