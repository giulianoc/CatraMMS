#!/bin/bash

if [ $# -ne 4 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <ingestion job key> <binary path name> ($#)"

	exit 1
fi

mmsUserKey=$1
mmsAPIKey=$2
ingestionJobKey=$3
binaryFilePathName=$4

#mmsBinaryHostname=mms-binary.catramms-cloud.com
mmsBinaryHostname=mms-binary.cibortv-mms.com

osName=$(uname -s)

startIngestion=$(date -u +%s)

responseCode=$(curl -k -s -o /dev/null -w "%{response_code}" -X POST -u $mmsUserKey:$mmsAPIKey -d @$binaryFilePathName "https://$mmsBinaryHostname/catramms/1.0.1/binary/$ingestionJobKey")

if [ "$responseCode" -ne "201" ]; then
	echo "$(date +%Y-%m-%d-%H:%M:%S): FAILURE, Ingestion response code: $responseCode, ingestionJobKey: $ingestionJobKey, binaryFilePathName: $binaryFilePathName"

	exit 2
fi

echo "Ingestion finished. Elapsed in seconds: $((endIngestion - startIngestion))"

exit 0

