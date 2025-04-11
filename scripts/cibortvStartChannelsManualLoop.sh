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

	type=restreamIt-manual

	#Questa curl viene chiamata all'interno di uno script e non dal crontab perché, nel caso in cui inpiegasse piu di un minuto,
	#potrebbero partire tante call in parallelo, e quindi i threads disponibili sul server micronaut si potrebbero saturare e provocare un disservizio.
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

