#!/bin/bash

debugFilename=/tmp/servicesStatus.log

debug=1

source /opt/catrasoftware/CatraMMS/scripts/servicesStatusLibrary.sh

while [ 1 ]
do

	before=$(date +%s)
	echo "" >> $debugFilename

	source /opt/catrasoftware/CatraMMS/scripts/servers.sh


	#PROD
	index=0
	while [ $index -lt $prodServersNumber ]
	do
		serverName=${prodServers[$((index*5+0))]}
		serverAddress=${prodServers[$((index*5+1))]}
		#serverKey=${prodServers[$((index*4+2))]}
		serverPort=${prodServers[$((index*5+3))]}

		echo "server_reachable serverName" >> $debugFilename
		server_reachable $serverAddress $serverPort $serverName

		index=$((index+1))
	done

	after=$(date +%s)

	elapsed=$((after-before))

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): prod elapsed: $elapsed secs" >> $debugFilename


	#TEST
	index=0
	while [ $index -lt $testServersNumber ]
	do
		serverName=${testServers[$((index*5+0))]}
		serverAddress=${testServers[$((index*5+1))]}
		#serverKey=${testServers[$((index*4+2))]}
		serverPort=${testServers[$((index*5+3))]}

		echo "server_reachable serverName" >> $debugFilename
		server_reachable $serverAddress $serverPort $serverName

		index=$((index+1))
	done

	after=$(date +%s)

	elapsed=$((after-before))

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): test elapsed: $elapsed secs" >> $debugFilename


	secondsToSleep=60
	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

