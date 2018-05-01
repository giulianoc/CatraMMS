#!/bin/bash

ingestionJobKey=$1
binaryFilePathName=$2
chunckSize=$3
ingestionNumberToBeContinued=$4

osName=$(uname -s)

if [ $# -eq 2 ]; then
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

	echo "chunckSize selected: $chunckSize"
	ingestionNumberToBeContinued=0
elif [ $# -eq 3 ]; then
	ingestionNumberToBeContinued=0
elif [ $# -ne 4 ]; then
	echo "Usage: $0 <ingestion job key> <binary path name> [<chunk size in bytes]"

	exit 1
fi

mmsHostName=mms-binary.catrasoft.cloud
#mmsPort=80
userKey=1
userAPIKey=SU1.8ZO1O2zRs.gL_nWYV4AZ0uU_dy89CRjqmaXv4J58R0NfNsa9Ku8f6OScAwS.gT
sleepingInSecondsInCaseOfIngestionError=5
maxRetriesNumber=3

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

			echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $ingestionNumber/$totalIngestionsNumber, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize"
			if [ "$osName" == "Darwin" ]; then
				echo "command: dd if=$binaryFilePathName bs=1 skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) 2> /dev/null | curl -s -o /dev/null -w \"%{response_code}\" -X POST -H \"$contentRange\" -u $userKey:$userAPIKey --data-binary @- \"http://$mmsHostName/catramms/binary/$ingestionJobKey\""
				responseCode=$(dd if=$binaryFilePathName bs=1 skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) 2> /dev/null | curl -s -o /dev/null -w "%{response_code}" -X POST -H "$contentRange" -u $userKey:$userAPIKey --data-binary @- "http://$mmsHostName/catramms/binary/$ingestionJobKey")
			else
				echo "command: dd status=none if=$binaryFilePathName bs=1024 iflag=skip_bytes,count_bytes skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) | curl -s -o /dev/null -w \"%{response_code}\" -X POST -H \"$contentRange\" -u $userKey:$userAPIKey --data-binary @- \"http://$mmsHostName/catramms/binary/$ingestionJobKey\""
				responseCode=$(dd status=none if=$binaryFilePathName bs=1024 iflag=skip_bytes,count_bytes skip=$contentRangeStart count=$((contentRangeEnd - contentRangeStart + 1)) | curl -s -o /dev/null -w "%{response_code}" -X POST -H "$contentRange" -u $userKey:$userAPIKey --data-binary @- "http://$mmsHostName/catramms/binary/$ingestionJobKey")
			fi

			#echo "responseCode: $responseCode"

			endChunkIngestion=$(date -u +%s)

			if [ "$responseCode" -ne "201" ]; then
				echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $ingestionNumber/$totalIngestionsNumber FAILED, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize, Ingestion response code: $responseCode. Elapsed (secs): $((endChunkIngestion - startChunkIngestion))"

				retriesNumber=$((retriesNumber + 1))

				echo "Pause $sleepingInSecondsInCaseOfIngestionError seconds..."
				sleep $sleepingInSecondsInCaseOfIngestionError
			else
				echo "$(date +%Y-%m-%d-%H:%M:%S): IngestionNumber $ingestionNumber/$totalIngestionsNumber SUCCESS, bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize. Elapsed (secs): $((endChunkIngestion - startChunkIngestion))"
			fi
		done

		if [ "$retriesNumber" -ge "$maxRetriesNumber" ]; then
			echo "FAILURE at ingestionNumber: $ingestionNumber (bytes $contentRangeStart-$contentRangeEnd/$binaryFileSize)"

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

