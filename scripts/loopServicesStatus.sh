#!/bin/bash

debugFilename=/tmp/servicesStatus.log

debug=1


if [ $# -ne 1 ]
then
	echo "usage $0 <moduleType <see servicesStatus..sh>" >> $debugFilename

	exit
fi

moduleType=$1

while [ 1 -eq 1 ]
do
	before=$(date +%s)
	/opt/catramms/CatraMMS/scripts/servicesStatus.sh $moduleType
	after=$(date +%s)

	elapsed=$((after-before))

	echo "elapsed: $elapsed secs, sleeping 30 secs" >> $debugFilename
	sleep 30
done

