#!/bin/bash

debugFile=/tmp/tvController.log

debug=1

while [ 1 -eq 1 ]
do
	/opt/catramms/CatraMMS/scripts/tvController.sh

	echo "sleeping 30 secs" >> $debugFile
	sleep 30
done

