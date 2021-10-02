#!/bin/bash

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 <catramms version, i.e.: 1.0.0>"

    exit
fi

version=$1


mmsStopALL.sh
sleep 2

currentPathNameVersion=$(readlink -f /opt/catramms/CatraMMS)

echo "cd /opt/catramms"
cd /opt/catramms

echo "rm -f CatraMMS"
rm -f CatraMMS

sleep 1

linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
#linuxName using centos will be "centos", next remove "
linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

echo "tar xvfz CatraMMS-$version-$linuxName.tar.gz"
tar xvfz CatraMMS-$version-$linuxName.tar.gz

echo "ln -s CatraMMS-$version CatraMMS"
ln -s CatraMMS-$version CatraMMS

sleep 1

cd

if [ "${currentPathNameVersion}" != "" ];
then
	tenDaysInMinutes=14400

	echo "Remove previous versions (retention $tenDaysInMinutes)"
	find /opt/catramms -mmin +$tenDaysInMinutes -name "CatraMMS-*" -not -path "${currentPathNameVersion}*" -exec rm -rf {} \;
fi

mmsStatusALL.sh

mmsStatusALL.sh

mmsStartALL.sh

