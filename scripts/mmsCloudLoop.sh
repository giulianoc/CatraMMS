#!/bin/bash

if [ $# -ne 1 ];
then
	echo "Usage $0 <env (prod or test)>"

	exit 1
fi

env=$1

debugFilename=/tmp/mmsCloudLoop.log

debug=1

source /opt/catrasoftware/CatraMMS/scripts/servicesStatusLibrary.sh

while [ 1 ]
do

	before=$(date +%s)
	echo "" >> $debugFilename

	source /opt/catrasoftware/CatraMMS/scripts/servers.sh


	#PROD
	if [ "$env" == "prod" ];
	then
		index=0
		while [ $index -lt $prodServersNumber ]
		do
			serverName=${prodServers[$((index*6+0))]}
			serverAddress=${prodServers[$((index*6+1))]}
			#serverKey=${prodServers[$((index*4+2))]}
			serverPort=${prodServers[$((index*6+3))]}

			echo "server_reachable serverName" >> $debugFilename
			server_reachable $serverAddress $serverPort $serverName

			index=$((index+1))
		done

		after=$(date +%s)

		elapsed=$((after-before))

		echo "" >> $debugFilename
		echo "$(date +'%Y/%m/%d %H:%M:%S'): prod elapsed: $elapsed secs" >> $debugFilename
	elif [ "$env" == "test" ];
	then
		#TEST
		index=0
		while [ $index -lt $testServersNumber ]
		do
			serverName=${testServers[$((index*6+0))]}
			serverAddress=${testServers[$((index*6+1))]}
			#serverKey=${testServers[$((index*4+2))]}
			serverPort=${testServers[$((index*6+3))]}

			echo "server_reachable serverName" >> $debugFilename
			server_reachable $serverAddress $serverPort $serverName

			index=$((index+1))
		done

		after=$(date +%s)

		elapsed=$((after-before))

		echo "" >> $debugFilename
		echo "$(date +'%Y/%m/%d %H:%M:%S'): test elapsed: $elapsed secs" >> $debugFilename
	else
		echo "$(date +'%Y/%m/%d %H:%M:%S'): wrong env: $env" >> $debugFilename
	fi

	secondsToSleep=60
	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

