#!/bin/bash

usage="$0 --engine --encoder --api --log 1 --sql 1 --mms 1 --ffmpeg 1 --cp 1 --mv 1 --rm 1"

options=$(getopt -n "$0"  -o h --longoptions "help,engine,encoder,api,log:,sql:,mms:,ffmpeg:,cp:,mv:,rm:" -- "$@")

#Bad arguments, something has gone wrong with the getopt command.
if [ $? -ne 0 ];
then
	echo "Usage $usage"

	exit 1
fi

# A little magic, necessary when using getopt.
eval set -- "$PARSED_OPTIONS"

components=""
logFileNumber=1
sqlThresholdInSeconds=1
mmsThresholdInSeconds=-1
copyThresholdInSeconds=1
moveThresholdInSeconds=1
removeThresholdInSeconds=1
ffmpegThresholdInSeconds=$((30*60))

# shift to analyse 1 argument at a time.
# $1 identifies the first argument, and when we use shift we discard the first argument,
#	so $2 becomes $1 and goes again through the case.
while true;
do
	case "$1" in
 
		-h|--help)
			echo "Usage $usage"
			shift;;
 
		--engine)
			echo "engine"
			components=$components" engine"
			shift;;
 
		--encoder)
			echo "encoder"
			components=$components" encoder"
			shift;;
 
		--api)
			echo "api"
			components=$components" api"
			shift;;
 
		--log)
			echo "log"
 
			if [ -n "$2" ];
			then
				echo "log: $2"
				logFileNumber=$2
			fi
			shift 2;;
 
		--sql)
			echo "sql"
 
			if [ -n "$2" ];
			then
				echo "sql: $2"
				sqlThresholdInSeconds=$2
			fi
			shift 2;;
 
		--cp)
			echo "cp"
 
			if [ -n "$2" ];
			then
				echo "cp: $2"
				copyThresholdInSeconds=$2
			fi
			shift 2;;

		--mv)
			echo "mv"
 
			if [ -n "$2" ];
			then
				echo "mv: $2"
				moveThresholdInSeconds=$2
			fi
			shift 2;;

		--rm)
			echo "rm"
 
			if [ -n "$2" ];
			then
				echo "rm: $2"
				removeThresholdInSeconds=$2
			fi
			shift 2;;

		--mms)
			echo "mms"
 
			if [ -n "$2" ];
			then
				echo "mms: $2"
				mmsThresholdInSeconds=$2
			fi
			shift 2;;

		--ffmpeg)
			echo "ffmpeg"
 
			if [ -n "$2" ];
			then
				echo "ffmpeg: $2"
				ffmpegThresholdInSeconds=$2
			fi
			shift 2;;

		--)
			shift
			break;;
	esac
done

if [ $# -gt 5 ]
then
        echo "Usage $usage"

        exit
fi

echo "components: $components, logFileNumber: $logFileNumber, sqlThresholdInSeconds: $sqlThresholdInSeconds"

for component in $components
do
	echo ""

	logFilePathName=$(printLogFileName.sh $component $logFileNumber)

	echo "$component --> $logFilePathName"

	if [ "$logFilePathName" != "" ]
	then
		if [ $sqlThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "SQL statistics"
			grep "@SQL statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v sqlThresholdInSeconds="$sqlThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > sqlThresholdInSeconds) printf("%s\n", $0) }'
		fi

		if [ $mmsThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "MMS statistics"
			grep "@MMS statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v mmsThresholdInSeconds="$mmsThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > mmsThresholdInSeconds) printf("%s\n", $0) }'
		fi

		if [ $copyThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "MMS COPY statistics"
			grep "@MMS COPY statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v mmsCopyMoveThresholdInSeconds="$mmsCopyMoveThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > mmsCopyMoveThresholdInSeconds) printf("%s\n", $0) }'
		fi

		if [ $moveThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "MMS MOVE statistics"
			grep "@MMS MOVE statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v mmsCopyMoveThresholdInSeconds="$mmsCopyMoveThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > mmsCopyMoveThresholdInSeconds) printf("%s\n", $0) }'
		fi

		if [ $removeThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "MMS REMOVE statistics"
			grep "@MMS REMOVE statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v mmsCopyMoveThresholdInSeconds="$mmsCopyMoveThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > mmsCopyMoveThresholdInSeconds) printf("%s\n", $0) }'
		fi

		if [ $ffmpegThresholdInSeconds -ne -1 ]
		then
			echo ""
			echo "FFMPEG statistics"
			grep "@FFMPEG statistics@" $(printLogFileName.sh $component $logFileNumber) | awk -v ffmpegThresholdInSeconds="$ffmpegThresholdInSeconds" 'BEGIN { FS="@" } { if($4 > ffmpegThresholdInSeconds) printf("%s\n", $0) }'
		fi

	fi
done

