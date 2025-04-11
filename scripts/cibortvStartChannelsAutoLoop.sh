#!/bin/bash

debugFilename=/tmp/startChannelsLoop.log

debug=1

while [ 1 ]
do

	if [ ! -f "$debugFilename" ]; then
		echo "" > $debugFilename
	else
		filesize=$(stat -c %s $debugFilename)
		if [ $filesize -gt 10000000 ]
		then
			echo "" > $debugFilename
		fi
	fi

	before=$(date +%s)

	type=restreamIt-auto

	#Questa curl viene chiamata all'interno di uno script e non dal crontab perché, anche a causa di brow, la call potrebbe
	#richiedere parecchio tempo (ad es. 300 secondi). Se venisse chiamata ogni minuto dal crontab, si avrebbero tante call in parallelo,
	#i threads disponibili sul server micronaut si potrebbero saturare e quindi provocare un disservizio.
	#Per evitare di dare fastidio agli altri server, poichè questo script viene chiamato dal server "utilizzato" solo per il DB,
	#sostituiamo https://cibortv.cibortv-mms.com con http://localhost:8084
	curl -s "http://localhost:8084/cibortv/rest/api-v1/startChannels?Type=$type&should_bypass_cache=true" > /dev/null

	after=$(date +%s)

	elapsed=$((after-before))

	secondsToSleep=60

	echo "" >> $debugFilename
	echo "$(date +'%Y/%m/%d %H:%M:%S'): $type elapsed: $elapsed secs, sleeping $secondsToSleep secs" >> $debugFilename

	sleep $secondsToSleep
done

