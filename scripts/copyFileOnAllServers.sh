#!/bin/bash


if [ $# -ne 2 ]
then
	echo "usage $0 <source file path name> <dest directory path name>"

	exit
fi

sourceFilePathName=$1
destDirectoryPathName=$2

source /opt/catrasoftware/CatraMMS/scripts/servers.sh

index=0
while [ $index -lt $prodServersNumber ]
do
	serverName=${prodServers[$((index*5+0))]}
	serverAddress=${prodServers[$((index*5+1))]}
	serverKey=${prodServers[$((index*5+2))]}
	serverPort=${prodServers[$((index*5+3))]}

	echo ""
	echo "serverName: $serverName"
	echo "scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName"
	scp -P $serverPort -i ~/ssh-keys/$serverKey.pem $sourceFilePathName mms@$serverAddress:$destDirectoryPathName

	index=$((index+1))
done

