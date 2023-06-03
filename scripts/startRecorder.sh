#!/bin/bash

help()
{
	echo "Usage $0
		[ -t | --env <prod or test> ]
		[ -u | --userKey <userKey> ]
		[ -a | --apiKey <apiKey> ]
		[ -k | --streamKey <i.e.: 1234> ]
		[ -m | --minutesToBeRun <i.e.: 60> ]
		[ -s | --startTime <date YY-%m-%dT%H:%M:%S> ]
		[ -e | --endTime <date YY-%m-%dT%H:%M:%S> ]
		[ -r | --retention <i.e.: 3y> ]
		[ -v | --virtualVOD <true or false> ]"

    exit 1
}

#Due punti singoli (:) : il valore è obbligatorio per questa opzione
#Due punti doppi (::) - Il valore è facoltativo
#Senza due punti : non sono richiesti valori
SHORT=t:,u:,a:,k:,m:,s:,e:,r:,v:,h
LONG=env:,userKey:,apiKey:,streamKey:,minutesToBeRun:,startTime:,endTime:,retention:,virtualVOD:,help
OPTS=$(getopt -a -n recorder --options $SHORT --longoptions $LONG -- "$@")

eval set -- "$OPTS"

VALID_ARGUMENTS=$# # Returns the count of arguments that are in short or long options

#echo ""
#echo ""
#echo $OPTS
#echo $VALID_ARGUMENTS

if [ "$VALID_ARGUMENTS" -eq 0 ]; then
  help
fi


env="prod"
minutesToBeRun=60
startTime=$(date +'%Y-%m-%dT%H:%M:%S')
endTime=$(date --date="+$minutesToBeRun minutes" +'%Y-%m-%dT%H:%M:%S')
virtualVOD=false

while :
do
	case "$1" in
		-t | --env )
			env="$2"
			shift 2
			;;
		-u | --userKey )
			userKey="$2"
			shift 2
			;;
		-a | --apiKey )
			apiKey="$2"
			shift 2
			;;
		-k | --streamKey )
			streamKey="$2"
			shift 2
			;;
		-m | --minutesToBeRun )
			minutesToBeRun="$2"
			startTime=$(date +'%Y-%m-%dT%H:%M:%S')
			endTime=$(date --date="+$minutesToBeRun minutes" +'%Y-%m-%dT%H:%M:%S')
			shift 2
			;;
		-s | --startTime )
			startTime="$2"
			shift 2
			;;
		-e | --endTime )
			endTime="$2"
			shift 2
			;;
		-r | --retention )
			retention="$2"
			shift 2
			;;
		-v | --virtualVOD )
			virtualVOD="$2"
			shift 2
			;;
		-h | --help)
			help
			;;
		--)
			shift;
			break
			;;
		*)
			echo "Unexpected option: $1"
			help
			;;
	esac
done

if [ -z "$userKey" ]
then
	echo "userKey is missing"
	help
elif [ -z "$apiKey" ]
then
	echo "apiKey is missing"
	help
elif [ -z "$streamKey" ]
then
	echo "streamKey is missing"
	help
fi

if [ "$env" == "test" ]
then
	hostname=mms-gui-test.catramms-cloud.com
else
	hostname=mms-gui.catramms-cloud.com
fi


#echo "curl \"https://$hostname/catramms/rest/api/liveRecorder/$streamKey/60?userKey=$userKey&apiKey=$apiKey&retention=$retention&thumbnail=true&virtualVOD=$virtualVOD&virtualVODMaxDurationInMinutes=60&monitoringFrameIncreasingEnabled=false&autoRenew=false&startRecording=$startTime&stopRecording=$endTime\""
#echo ""
curl "https://$hostname/catramms/rest/api/liveRecorder/$streamKey/60?userKey=$userKey&apiKey=$apiKey&retention=$retention&thumbnail=true&virtualVOD=$virtualVOD&virtualVODMaxDurationInMinutes=60&monitoringFrameIncreasingEnabled=false&autoRenew=false&startRecording=$startTime&stopRecording=$endTime"

