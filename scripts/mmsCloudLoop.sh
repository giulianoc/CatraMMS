#!/bin/bash

debugFilename=/tmp/servicesStatus.log

debug=1

source /opt/catrasoftware/CatraMMS/scripts/servicesStatusLibrary.sh

while [ 1 ]
do

	before=$(date +%s)
	echo "" >> $debugFilename

	source /opt/catrasoftware/CatraMMS/scripts/servers.sh

	index=0
	while [ $index -lt $prodServersNumber ]
	do
		serverName=${prodServers[$((index*4+0))]}
		serverAddress=${prodServers[$((index*4+1))]}
		#serverKey=${prodServers[$((index*4+2))]}
		serverPort=${prodServers[$((index*4+3))]}

		echo "server_reachable serverName" >> $debugFilename
		server_reachable $serverAddress $serverPort $serverName

		index=$((index+1))
	done

	after=$(date +%s)

	elapsed=$((after-before))

	secondsToSleep=60

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): script elapsed: $elapsed secs, sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

