#!/bin/bash

if [ $# -ne 6 ];
then
    echo "Usage $0 <userKey> <apiKey> <streamKey> <minutes to be run> <retention: i.e. 3M (month)> <virtual VOD: i.e. true>"

    exit 1
fi

USER_KEY=$1
API_KEY=$2
STREAM_KEY=$3
MINUTES_TO_BE_RUN=$4
RETENTION=$5
VIRTUALVOD=$6

startTime=$(date +'%Y-%m-%dT%H:%M:%S')
endTime=$(date --date="+$MINUTES_TO_BE_RUN minutes" +'%Y-%m-%dT%H:%M:%S')

#echo $startTime
#echo $endTime

echo "curl \"https://mms-gui.catramms-cloud.com/catramms/rest/api/liveRecorder/$STREAM_KEY/60?userKey=$USER_KEY&apiKey=$API_KEY&retention=$RETENTION&thumbnail=true&virtualVOD=$VIRTUALVOD&virtualVODMaxDurationInMinutes=60&monitoringFrameIncreasingEnabled=false&autoRenew=false&startRecording=$startTime&stopRecording=$endTime\""
echo ""
curl "https://mms-gui.catramms-cloud.com/catramms/rest/api/liveRecorder/$STREAM_KEY/60?userKey=$USER_KEY&apiKey=$API_KEY&retention=$RETENTION&thumbnail=true&virtualVOD=$VIRTUALVOD&virtualVODMaxDurationInMinutes=60&monitoringFrameIncreasingEnabled=false&autoRenew=false&startRecording=$startTime&stopRecording=$endTime"

