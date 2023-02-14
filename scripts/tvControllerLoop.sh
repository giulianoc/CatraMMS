#!/bin/bash

debugFile=/tmp/tvController.log

debug=1

while [ 1 -eq 1 ]
do
	filesize=$(stat -c %s $debugFile)
	if [ $filesize -gt 1000000 ]
	then
		echo "" > $debugFile
	fi

	/opt/catramms/CatraMMS/scripts/tvController.sh

	echo "sleeping 30 secs" >> $debugFile
	sleep 30
done

