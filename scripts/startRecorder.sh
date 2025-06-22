#!/bin/bash

help()
{
	echo "Usage $0
		[ -t | --env <prod (default) or test> ]
		[ -u | --userKey <userKey> ]
		[ -a | --apiKey <apiKey> ]
		[ -k | --streamKey <i.e.: 1234> ]
		[ -m | --minutesToBeRun <default: 60> ]
		[ -s | --startTime <date %Y-%m-%dT%H:%M:%S> ]
		[ -e | --endTime <date %Y-%m-%dT%H:%M:%S> ]
		[ -r | --retention <i.e.: 3y> ]
		[ -v | --virtualVOD <default: false> ]
		[ -n | --autoRenew <default: false> ]
		[ -i | --utcTimeOverlay <default: false> ]
		[ -c | --cdn77ChannelConfigurationLabel <i.e.: camera_1_main> ]
		[ -o | --encodingProfileForCDN <i.e.: MMS_HLS_H264_2500Kb_medium_1080p25_high422_AAC_160> ]"

    exit 1
}

#Due punti singoli (:) : il valore è obbligatorio per questa opzione
#Due punti doppi (::) - Il valore è facoltativo
#Senza due punti : non sono richiesti valori
SHORT=t:,u:,a:,k:,m:,s:,e:,r:,v:,n:,i:,c:,o:,h
LONG=env:,userKey:,apiKey:,streamKey:,minutesToBeRun:,startTime:,endTime:,retention:,virtualVOD:,autoRenew:,utcTimeOverlay:,cdn77ChannelConfigurationLabel:,encodingProfileForCDN:,help
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
autoRenew=false
utcTimeOverlay=false
cdn77ChannelConfigurationLabel=
encodingProfileForCDN=

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
		-n | --autoRenew )
			autoRenew="$2"
			shift 2
			;;
		-i | --utcTimeOverlay )
			utcTimeOverlay="$2"
			shift 2
			;;
		-c | --cdn77ChannelConfigurationLabel )
			cdn77ChannelConfigurationLabel="$2"
			shift 2
			;;
		-o | --encodingProfileForCDN )
			encodingProfileForCDN="$2"
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
	hostname=mms-webapi-test.catramms-cloud.com
else
	hostname=mms-webapi.catramms-cloud.com
fi


curl "https://$hostname/catramms/webapi/1.0.0/liveRecorder/$streamKey/60?userKey=$userKey&apiKey=$apiKey&retention=$retention&thumbnail=true&virtualVOD=$virtualVOD&virtualVODMaxDurationInMinutes=60&cdn77ChannelConfigurationLabel=$cdn77ChannelConfigurationLabel&encodingProfileForCDN=$encodingProfileForCDN&monitoringFrameIncreasingEnabled=false&autoRenew=$autoRenew&utcTimeOverlay=$utcTimeOverlay&startRecording=$startTime&stopRecording=$endTime"

