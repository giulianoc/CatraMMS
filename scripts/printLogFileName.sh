#!/bin/bash

if [ $# -ne 1 -a $# -ne 2 ]
then
        echo "Usage $0 engine | encoder | api [logFileNumber]"

        exit
fi

component=$1
if [ $# -eq 2 ]
then
    logFileNumber=$2
	if [ $logFileNumber -lt 0 ]
	then
		logFileNumber=$((logFileNumber*-1))
	fi
else
    logFileNumber=1
fi


if [ "$component" == "engine" ]
then
        #lastLogFile=$(find ~/logs/mmsEngineService/ -type f | sort -r | head -n 1)
        lastLogFile=$(find ~/logs/mmsEngineService/ -type f -exec stat -c '%X %n' {} \; | sort -nr | awk -v logFileNumber=$logFileNumber 'BEGIN{FS=" "} { if (NR == logFileNumber) printf("%s\n", $2) }' )
elif [ "$component" == "encoder" ]
then
        lastLogFile=$(find ~/logs/mmsEncoder/ -type f -exec stat -c '%X %n' {} \; | sort -nr | awk -v logFileNumber=$logFileNumber 'BEGIN{FS=" "} { if (NR == logFileNumber) printf("%s\n", $2) }' )
elif [ "$component" == "api" ]
then
        lastLogFile=$(find ~/logs/mmsAPI/ -type f -exec stat -c '%X %n' {} \; | sort -nr | awk -v logFileNumber=$logFileNumber 'BEGIN{FS=" "} { if (NR == logFileNumber) printf("%s\n", $2) }' )
elif [ "$component" == "tomcat" ]
then
        lastLogFile="/home/mms/logs/tomcat-gui/catalina.out /home/mms/logs/tomcat-gui/catramms.log /home/mms/logs/tomcat-gui/catrammslib.log /home/mms/logs/tomcat-gui/catramms-web-services.log"
else
        echo "wrong input: $component"

        exit
fi

echo "$lastLogFile"


