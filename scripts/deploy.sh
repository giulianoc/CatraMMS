#!/bin/bash

if [ $# -ne 1 ]
then
    echo "$(date): usage $0 <mms version, i.e.: 1.0.0>"

    exit
fi

version=$1

sleepIfNeeded()
{
	currentSeconds=$(date +"%-S")
	if [ $currentSeconds -gt 45 ]
	then
		secondsToSleep=$((60-$currentSeconds+10))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	elif [ $currentSeconds -lt 10 ]
	then
		secondsToSleep=$((10-$currentSeconds))

		echo "Current seconds: $currentSeconds, sleeping $secondsToSleep"
		sleep $secondsToSleep
	fi
}

removePreviousVersions()
{
	currentPathNameVersion=$(readlink -f /opt/mms/MMS)
	if [ "${currentPathNameVersion}" != "" ];
	then
		tenDaysInMinutes=14400

		echo ""
		echo "------------------------------------------------------------------------------------------------------------------------------------------"
		echo "Remove previous versions (retention $tenDaysInMinutes)"
		echo "find /opt/mms -maxdepth 1 -mmin +$tenDaysInMinutes -name \"MMS-*\" -not -path \"${currentPathNameVersion}*\" -exec rm -rf {} \;"
		find /opt/mms -maxdepth 1 -mmin +$tenDaysInMinutes -name "MMS-*" -not -path "${currentPathNameVersion}*" -exec rm -rf {} \;
	fi
}


#linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
##linuxName using centos will be "centos", next remove "
#linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

#if [ ! -f "/opt/mms/MMS-$version-$linuxName.tar.gz" ]; then
#    echo "/opt/mms/MMS-$version-$linuxName.tar.gz does not exist."

#	exit
#fi
if [ ! -f "/opt/mms/MMS-$version.tar.gz" ]; then
    echo "/opt/mms/MMS-$version.tar.gz does not exist."

	exit
fi

#sleepIfNeeded
removePreviousVersions

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStopAll.sh"
~/mmsStopALL.sh

echo "cd /opt/mms"
cd /opt/mms

echo "rm -f MMS"
rm -f MMS

sleep 1

#echo "tar xvfz MMS-$version-$linuxName.tar.gz"
#tar xvfz MMS-$version-$linuxName.tar.gz
echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "tar xfz MMS-$version.tar.gz"
tar xfz MMS-$version.tar.gz

echo ""
echo ""
echo "ln -s MMS-$version MMS"
ln -s MMS-$version MMS

cd

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStatusALL.sh"
/home/mms/mmsStatusALL.sh

#per il transcoder serve un po piuu di tempo
sleep 3

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStatusALL.sh"
/home/mms/mmsStatusALL.sh

echo ""
echo "------------------------------------------------------------------------------------------------------------------------------------------"
echo "mmsStartALL.sh"
/home/mms/mmsStartALL.sh

