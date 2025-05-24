#!/bin/bash


if [ $# -ne 3 ]
then
	echo "usage $0 <env: prod or test> <source file path name> <dest directory path name>"

	exit
fi

env=$1
sourceFilePathName=$2
destDirectoryPathName=$3

source /opt/catrasoftware/CatraMMS/scripts/servers.sh

if [ "$env" == "prod" ];
then
	index=0
	while [ $index -lt $prodServersNumber ]
	do
		serverName=${prodServers[$((index*6+0))]}
		serverAddress=${prodServers[$((index*6+1))]}
		serverKey=${prodServers[$((index*6+2))]}
		serverPort=${prodServers[$((index*6+3))]}
		serverType=${prodServers[$((index*6+4))]}

		if [ "$serverType" == "storage" ]; then
			continue
		fi

		echo ""
		echo "serverName: $serverName"
		echo "scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName"
		scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName

		index=$((index+1))
	done
elif [ "$env" == "test" ];
then
	index=0
	while [ $index -lt $testServersNumber ]
	do
		serverName=${testServers[$((index*6+0))]}
		serverAddress=${testServers[$((index*6+1))]}
		serverKey=${testServers[$((index*6+2))]}
		serverPort=${testServers[$((index*6+3))]}
		serverType=${testServers[$((index*6+4))]}

		if [ "$serverType" == "storage" ]; then
			continue
		fi

		echo ""
		echo "serverName: $serverName"
		echo "scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName"
		scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName

		index=$((index+1))
	done
else
	echo "$(date +'%Y/%m/%d %H:%M:%S'): wrong env: $env"
fi
