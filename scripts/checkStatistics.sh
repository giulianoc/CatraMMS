#!/bin/bash

if [ $# -gt 3 ]
then
        echo "Usage $0 [engine | encoder | api][logFileNumber][threshold in seconds]"

        exit
fi

if [ $# -eq 0 ]
then
	components="engine encoder api"
    logFileNumber=1
    thresholdInSeconds=1
elif [ $# -eq 1 ]
then
	components=$1
    logFileNumber=1
    thresholdInSeconds=1
elif [ $# -eq 2 ]
then
	components=$1
    logFileNumber=$2
    thresholdInSeconds=1
elif [ $# -eq 3 ]
then
	components=$1
    logFileNumber=$2
    thresholdInSeconds=$3
fi

echo "components: $components, logFileNumber: $logFileNumber, thresholdInSeconds: $thresholdInSeconds"

for component in $components
do
	echo ""
	echo "$component ..."

	grep "SQL statistics" $(printLogFileName.sh $component $logFileNumber) | awk -v thresholdInSeconds="$thresholdInSeconds" 'BEGIN { FS="@" } { if($2 > thresholdInSeconds) printf("%s\n", $0) }'
done

