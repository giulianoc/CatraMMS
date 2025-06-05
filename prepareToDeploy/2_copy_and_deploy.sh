#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
	echo "Usage: $0 <serverType: api, delivery, engine, encoder, externalEncoder> <servername>"
	exit 1
fi
requestedServerType=$1
requestedServerName=$2

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

	echo ""
	echo ""
	echo $serverName
	scp -P $serverPort -i ~/ssh-keys/$serverKey.pem /opt/catrasoftware/deploy/$tarFileName mms@$serverAddress:/opt/catramms
	echo ""

	echo ""
	echo ""
	echo "deploy..."
	if ! ssh -p $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "[ -f '/opt/catramms/CatraMMS/scripts/deploy.sh' ]"; then
		ssh -p $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "tar xfz /opt/catramms/$tarFileName -C /opt/catramms"
		ssh -p $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "ln -s /opt/catramms/$moduleName-$version /opt/catramms/CatraMMS"
	fi
	ssh -p $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "/opt/catramms/CatraMMS/scripts/deploy.sh $version"

	#if [ "$serverType" = "api" -o "$serverType" = "api-and-delivery" -o "$serverType" = "delivery" ]; then
	#	tailCommand="tail -f logs/mmsAPI/mmsAPI-error.log"
	#elif [ "$serverType" = "engine" ]; then
	#	tailCommand="tail -f logs/mmsEngineService/mmsEngineService-error.log"
	#elif [ "$serverType" = "encoder" -o "$serverType" = "externalEncoder" ]; then
	#	tailCommand="tail -f logs/mmsEncoder/mmsEncoder-error.log"
	#fi
	#if [ "$tailCommand" != "" ]; then
	#	echo ""
	#	echo ""
	#	echo "tail on errors..."
	#	ssh -p $serverPort -i ~/ssh-keys/$serverKey.pem mms@$serverAddress "$tailCommand"
	#fi
}

echo -n "deploy su mms cloud/test? " 
read deploy
if [ "$deploy" == "y" ]; then
	index=0
	while [ $index -lt $testServersNumber ]
	do
		serverName=${testServers[$((index*6+0))]}
		serverAddress=${testServers[$((index*6+1))]}
		serverKey=${testServers[$((index*6+2))]}
		serverPort=${testServers[$((index*6+3))]}
		serverType=${testServers[$((index*6+4))]}
		serverPrivateIP=${testServers[$((index*6+5))]}

		if [ "$requestedServerType" != "" ] && [[ ! "$serverType" == *"$requestedServerType"* ]]; then
			# entra se requestedServerType != "" e requestedServerType non è contenuto in serverType
			index=$((index+1))
			continue
		fi

		if [ "$requestedServerName" != "" -a "$requestedServerName" != "$serverName" ]; then
			index=$((index+1))
			continue
		fi

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
		serverName=${prodServers[$((index*6+0))]}
		serverAddress=${prodServers[$((index*6+1))]}
		serverKey=${prodServers[$((index*6+2))]}
		serverPort=${prodServers[$((index*6+3))]}
		serverType=${prodServers[$((index*6+4))]}
		serverPrivateIP=${prodServers[$((index*6+5))]}

		if [ "$serverType" == "integration" ]; then
			index=$((index+1))
			continue
		fi

		if [ "$requestedServerType" != "" ] && [[ ! "$serverType" == *"$requestedServerType"* ]]; then
			# entra se requestedServerType != "" e requestedServerType non è contenuto in serverType
			index=$((index+1))
			continue
		fi

		if [ "$requestedServerName" != "" -a "$requestedServerName" != "$serverName" ]; then
			index=$((index+1))
			continue
		fi

		#if [ "$serverType" = "api" ]; then
		#	echo ""
		#	echo ""
		#	echo "Remove server from the load balancer: hcloud load-balancer remove-target --ip $serverPrivateIP mms-api-prod"
		#	hcloud load-balancer remove-target --ip $serverPrivateIP mms-api-prod
		#	echo "Waiting load balancer command..."
		#	sleep 5
		#fi
		deploy $serverName $serverAddress $serverPort $serverKey $serverType
		#if [ "$serverType" = "api" ]; then
		#	echo ""
		#	echo ""
		#	echo "Add server to the load balancer: hcloud load-balancer add-target --ip $serverPrivateIP mms-api-prod"
		#	hcloud load-balancer add-target --ip $serverPrivateIP mms-api-prod
		#	echo "Waiting load balancer command..."
		#	sleep 5
		#fi

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
	#delivery-binary-gui-2
	scp -P 9255 -i ~/ssh-keys/hetzner-mms-key.pem /opt/catrasoftware/deploy/$tarFileName mms@116.202.53.105:/var/catramms/storage/MMSRepository-free/packages/ubuntu-24.04
fi

