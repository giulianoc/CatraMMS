#!/bin/bash

debugFilename=/tmp/servicesStatus.log

debug=1


if [ $# -eq 0 ]
then
	echo "see serviceStatusAgent.sh parameters" >> $debugFilename

	exit
fi

while [ 1 -eq 1 ]
do
	before=$(date +%s)
	/opt/catramms/CatraMMS/scripts/servicesStatusAgent.sh $*
	after=$(date +%s)

	elapsed=$((after-before))

	secondsToSleep=60

	echo "$(date +'%Y/%m/%d %H:%M:%S'): script elapsed: $elapsed secs, sleeping $secondsToSleep secs" >> $debugFilename
	sleep $secondsToSleep
done

