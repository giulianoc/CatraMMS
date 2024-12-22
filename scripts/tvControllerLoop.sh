#!/bin/bash

#da aggiungere nel crontab di root
#*/1 * * * * pgrep tvControllerL > /dev/null; if [ $? -ne 0 ]; then nohup /opt/catramms/CatraMMS/scripts/tvControllerLoop.sh & fi;

debugFile=/tmp/tvController.log

debug=1

while [ 1 -eq 1 ]
do
	/opt/catramms/CatraMMS/scripts/tvController.sh

	secondsToSleep=30
	echo "sleeping $secondsToSleep secs" >> $debugFile
	sleep $secondsToSleep
done

