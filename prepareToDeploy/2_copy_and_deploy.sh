#!/bin/bash

date

version=$(cat ./version.txt)

currentDir=$(pwd)
moduleName=$(basename $currentDir)

#linuxName=$(cat /etc/os-release | grep "^ID=" | cut -d'=' -f2)
##linuxName using centos will be "centos", next remove "
#linuxName=$(echo $linuxName | awk '{ if (substr($0, 0, 1) == "\"") printf("%s", substr($0, 2, length($0) - 2)); else printf("%s", $0) }')

#tarFileName=$moduleName-$version-$linuxName.tar.gz
tarFileName=$moduleName-$version.tar.gz

source /opt/catrasoftware/CatraMMS/scripts/servers.sh

deploy()
{
	serverName=$1
	serverAddress=$2
	serverPort=$3
	serverKey=$4
	serverType=$5

	#tarFileName e version sono già inizializzate (vedi sopra)

	echo $serverName
	scp -P $serverPort -i ~/ssh-keys/$serverKey.pem /opt/catrasoftware/deploy/$tarFileName mms@$serverAddress:/opt/catramms
	echo ""

	ssh -P $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "/opt/catramms/CatraMMS/scripts/deploy.sh $version"

	if [ "$serverType" = "api" -o "$serverType" = "api-and-delivery" -o "$serverType" = "delivery" ]
		tailCommand="tail -f logs/mmsAPI/mmsAPI-error.log"
	elif [ "$serverType" = "engine" ]
		tailCommand="tail -f logs/mmsEngineService/mmsEngineService-error.log"
	elif [ "$serverType" = "encoder" -o "$serverType" = "externalEncoder" ]
		tailCommand="tail -f logs/mmsEncoder/mmsEncoder-error.log"
	fi
	if [ "$tailCommand" != "" ]
		ssh -P $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "tail -f logs/mmsAPI/mmsAPI.log"
}

echo -n "deploy su mms cloud/test? " 
read deploy
if [ "$deploy" == "y" ]; then
	index=0
	while [ $index -lt $testServersNumber ]
	do
		serverName=${testServers[$((index*5+0))]}
		serverAddress=${testServers[$((index*5+1))]}
		serverKey=${testServers[$((index*5+2))]}
		serverPort=${testServers[$((index*5+3))]}
		serverType=${testServers[$((index*5+4))]}

		deploy $serverName $serverAddress $serverPort $serverKey $serverType

		index=$((index+1))
	done
fi

echo ""
echo -n "deploy su cloud production? " 
read deploy
if [ "$deploy" == "y" ]; then
	index=0
	while [ $index -lt $prodServersNumber ]
	do
		serverName=${prodServers[$((index*5+0))]}
		serverAddress=${prodServers[$((index*5+1))]}
		serverKey=${prodServers[$((index*5+2))]}
		serverPort=${prodServers[$((index*5+3))]}
		serverType=${prodServers[$((index*5+4))]}

		if [ "$serverType" == "integration" ]; then
			index=$((index+1))
			continue
		fi

		echo $serverName
		scp -P $serverPort -i ~/ssh-keys/$serverKey.pem /opt/catrasoftware/deploy/$tarFileName mms@$serverAddress:/opt/catramms
		echo ""

		ssh -P $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "/opt/catramms/CatraMMS/scripts/deploy.sh $version"

		index=$((index+1))
	done
fi

#echo -n "Load package to MMSRepository-free (ubuntu 22.04)? " 
#read deploy
#if [ "$deploy" == "y" ]; then
#	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@168.119.250.162:/var/catramms/storage/MMSRepository-free/packages/ubuntu-22.04
#fi

echo -n "Load package to MMSRepository-free (ubuntu 24.04)? " 
read deploy
if [ "$deploy" == "y" ]; then
	#engine-db-1
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@167.235.10.244:/mnt/mmsStorage/MMSRepository-free/packages/ubuntu-24.04
fi

