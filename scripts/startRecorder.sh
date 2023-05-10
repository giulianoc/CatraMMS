#!/bin/bash

if [ $# -ne 4 ];
then
    echo "Usage $0 <userKey> <apiKey> <channelKey> <minutes to be run>"

    exit 1
fi

USER_KEY=$1
API_KEY=$2
STREAM_KEY=$3
MINUTES_TO_BE_RUN=$4

startTime=$(date +'\%Y-\%m-\%dT\%H:\%M:\%S')
endTime=$(date --date="+$MINUTES_TO_BE_RUN seconds" +'\%Y-\%m-\%dT\%H:\%M:\%S')

#echo $startTime
#echo $endTime

curl "https://mms-gui.catramms-cloud.com/catramms/rest/api/liveRecorder/$STREAM_KEY/60?userKey=$USER_KEY&apiKey=$API_KEY&retention=10d&thumbnail=true&virtualVOD=true&virtualVODMaxDurationInMinutes=60&monitoringFrameIncreasingEnabled=false&autoRenew=false&startRecording=$startTime&stopRecording=$endTime"

