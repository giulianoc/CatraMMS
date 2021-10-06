#!/bin/bash

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 <catramms version, i.e.: 1.0.0>"

    exit
fi

version=$1

sleepIfNeeded()
{
	currentSeconds=$(date +"%-S")
	if [ $currentSeconds -gt 45 ]
	then
		secondsToSleep=$((60-$currentSeconds+15))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	elif [ $currentSeconds -lt 15 ]
	then
		secondsToSleep=$((15-$currentSeconds))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	fi
}

removePreviousVersions()
{
	currentPathNameVersion=$(readlink -f /opt/catramms/CatraMMS)
	if [ "${currentPathNameVersion}" != "" ];
	then
		tenDaysInMinutes=14400

		echo "Remove previous versions (retention $tenDaysInMinutes)"
		find /opt/catramms -maxdepth 1 -mmin +$tenDaysInMinutes -name "CatraMMS-*" -not -path "${currentPathNameVersion}*" -exec rm -rf {} \;
	fi
}


linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
#linuxName using centos will be "centos", next remove "
linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

if [ ! -f "/opt/catramms/CatraMMS-$version-$linuxName.tar.gz" ]; then
    echo "/opt/catramms/CatraMMS-$version-$linuxName.tar.gz does not exist."

	exit
fi

sleepIfNeeded
removePreviousVersions

mmsStopALL.sh
sleep 2

echo "cd /opt/catramms"
cd /opt/catramms

echo "rm -f CatraMMS"
rm -f CatraMMS

sleep 1

echo "tar xvfz CatraMMS-$version-$linuxName.tar.gz"
tar xvfz CatraMMS-$version-$linuxName.tar.gz

echo "ln -s CatraMMS-$version CatraMMS"
ln -s CatraMMS-$version CatraMMS

sleep 1

cd

mmsStatusALL.sh

mmsStatusALL.sh

mmsStartALL.sh

