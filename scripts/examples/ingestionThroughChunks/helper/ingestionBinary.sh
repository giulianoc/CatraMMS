#!/bin/bash

mmsUserKey=$1
mmsAPIKey=$2
ingestionJobKey=$3
binaryFilePathName=$4
ingestionNumberToBeContinued=$5
chunckSize=$6

mmsBinaryHostname=mms-binary.catramms-cloud.com

osName=$(uname -s)

if [ $# -eq 4 ]; then
	ingestionNumberToBeContinued=0

	if [ "$osName" == "Darwin" ]; then
		binaryFileSize=$(stat -f%z "$binaryFilePathName")
	else
		binaryFileSize=$(stat -c%s "$binaryFilePathName")
	fi
	if [ $binaryFileSize -lt 100000 ]; then			#100KB
		chunckSize=10000			#10KB
	elif [ $binaryFileSize -lt 10000000 ]; then		#10MB
		chunckSize=1000000			#1MB
	elif [ $binaryFileSize -lt 100000000 ]; then	#100MB
		chunckSize=10000000			#10MB
	elif [ $binaryFileSize -lt 1000000000 ]; then	#1GB
		chunckSize=100000000		#100MB
	else
		chunckSize=100000000		#100MB
	fi

	#echo "chunckSize selected: $chunckSize"
elif [ $# -eq 5 ]; then
	if [ "$osName" == "Darwin" ]; then
		binaryFileSize=$(stat -f%z "$binaryFilePathName")
	else
		binaryFileSize=$(stat -c%s "$binaryFilePathName")
	fi
	if [ $binaryFileSize -lt 100000 ]; then			#100KB
		chunckSize=10000			#10KB
	elif [ $binaryFileSize -lt 10000000 ]; then		#10MB
		chunckSize=1000000			#1MB
	elif [ $binaryFileSize -lt 100000000 ]; then	#100MB
		chunckSize=10000000			#10MB
	elif [ $binaryFileSize -lt 1000000000 ]; then	#1GB
		chunckSize=100000000		#100MB
	else
		chunckSize=100000000		#100MB
	fi

	#echo "chunckSize selected: $chunckSize"
elif [ $# -ne 6 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <ingestion job key> <binary path name> [<chunk size in bytes]"

	exit 1
fi

sleepingInSecondsInCaseOfIngestionError=60
maxRetriesNumber=5

if [ "$osName" == "Darwin" ]; then
	binaryFileSize=$(stat -f%z "$binaryFilePathName")
else
	binaryFileSize=$(stat -c%s "$binaryFilePathName")
fi
totalIngestionsNumber=$((binaryFileSize / chunckSize))
if [ $((binaryFileSize % chunckSize)) -ne 0 ]; then
	totalIngestionsNumber=$((totalIngestionsNumber + 1))
fi

#echo "binaryFileSize: $binaryFileSize"
#echo "totalIngestionsNumber: $totalIngestionsNumber"

startIngestion=$(date -u +%s)

ingestionNumber=0
contentRangeStart=0
contentRangeEnd=0
while [  $ingestionNumber -lt $totalIngestionsNumber ]; do

	remainingBytes=$((binaryFileSize - contentRangeEnd))
	if [ $contentRangeEnd -ne 0 -a $remainingBytes -lt $chunckSize ]; then
		contentRangeEnd=$((contentRangeStart + remainingBytes - 2))
	else
		contentRangeEnd=$((contentRangeStart + chunckSize - 1))
	fi

	contentRange="Content-Range: bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize"

	if [ $ingestionNumber -ge $ingestionNumberToBeContinued ]; then
		responseCode="400"
		retriesNumber="0"
		while [ "$responseCode" -ne "201" -a "$retriesNumber" -lt "$maxRetriesNumber" ]; do

			startChunkIngestion=$(date -u +%s)

			echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $((ingestionNumber + 1))/$totalIngestionsNumber, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize"
			if [ "$osName" == "Darwin" ]; then
				#echo "command: dd if=\"$binaryFilePathName\" bs=1 skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) 2> /dev/null | curl -k -s -o /dev/null -w \"%{response_code}\" -X POST -H \"$contentRange\" -u $mmsUserKey:$mmsAPIKey --data-binary @- \"https://$mmsBinaryHostname/catramms/binary/1.0.1/$ingestionJobKey\""
				responseCode=$(dd if="$binaryFilePathName" bs=1 skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) 2> /dev/null | curl -k -s -o /dev/null -w "%{response_code}" -X POST -H "$contentRange" -u $mmsUserKey:$mmsAPIKey --data-binary @- "https://$mmsBinaryHostname/catramms/1.0.1/binary/$ingestionJobKey")
			else
				#echo "command: dd status=none if=\"$binaryFilePathName\" bs=1024 iflag=skip_bytes,count_bytes skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) | curl -k -s -o /dev/null -w \"%{response_code}\" -X POST -H \"$contentRange\" -u $mmsUserKey:$mmsAPIKey --data-binary @- \"https://$mmsBinaryHostname/catramms/1.0.1/binary/$ingestionJobKey\""
				responseCode=$(dd status=none if="$binaryFilePathName" bs=1024 iflag=skip_bytes,count_bytes skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) | curl -k -s -o /dev/null -w "%{response_code}" -X POST -H "$contentRange" -u $mmsUserKey:$mmsAPIKey --data-binary @- "https://$mmsBinaryHostname/catramms/1.0.1/binary/$ingestionJobKey")
			fi

			#echo "responseCode: $responseCode"

			endChunkIngestion=$(date -u +%s)

			if [ "$responseCode" -ne "201" ]; then
				echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $((ingestionNumber + 1))/$totalIngestionsNumber FAILED, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize, Ingestion response code: $responseCode. Elapsed (secs): $((endChunkIngestion - startChunkIngestion))"

				retriesNumber=$((retriesNumber + 1))

				echo "Pause $sleepingInSecondsInCaseOfIngestionError seconds and will retry again..."
				sleep $sleepingInSecondsInCaseOfIngestionError
			fi
			#else
				#echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $((ingestionNumber + 1))/$totalIngestionsNumber SUCCESS, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize. Elapsed (secs): $((endChunkIngestion - startChunkIngestion))"
			#fi
		done

		if [ "$retriesNumber" -ge "$maxRetriesNumber" ]; then
			echo "FAILURE at ingestionNumber $((ingestionNumber + 1)) (bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize), ingestionJobKey: $ingestionJobKey, binaryFilePathName: $binaryFilePathName"

			exit 2
		fi

		echo ""
	fi

	contentRangeStart=$((contentRangeEnd + 1))

	ingestionNumber=$((ingestionNumber + 1))
done

endIngestion=$(date -u +%s)

echo "Ingestion finished. Elapsed in seconds: $((endIngestion - startIngestion))"

exit 0

