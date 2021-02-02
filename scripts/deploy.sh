#!/bin/bash

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 <catramms version, i.e.: 1.0.0>"

    exit
fi

version=$1


mmsStopALL.sh
sleep 2


echo "cd /opt/catramms"
cd /opt/catramms

echo "rm -f CatraMMS"
rm -f CatraMMS

sleep 1

echo "tar xvfz CatraMMS-$version-ubuntu.tar.gz"
tar xvfz CatraMMS-$version-ubuntu.tar.gz

echo "ln -s CatraMMS-$version CatraMMS"
ln -s CatraMMS-$version CatraMMS

sleep 1

cd

mmsStatusALL.sh

mmsStatusALL.sh

mmsStartALL.sh

