#!/bin/bash                                                                                                   

if [ $# -ne 4 ];
then
	echo "Usage $0 <tsv path name> <userKey> <apiKey> <mmsHostname> (i.e.: 40 702347803978348... mms-api.cloud-mms.com)"

	exit 1
fi

tsvPathName=$1
userKey=$2
apiKey=$3
mmsApiHostname=$4

awk -v userKey=$userKey -v apiKey=$apiKey -v mmsApiHostname=$mmsApiHostname -f ./utility/foglio.awk $tsvPathName

