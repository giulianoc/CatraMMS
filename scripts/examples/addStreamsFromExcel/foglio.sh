#!/bin/bash                                                                                                   

if [ $# -ne 4 ];
then
	echo "Usage $0 <tsv path name> <userKey> <apiKey> <mmsAPIHostname> (i.e.: 1 HNVOoVhHx0yoWNIxFu-ThBA1vAPEKWsxneGgze6eoddEv9aB1xp9NzsBKDBDMEFN mms-api.cibortv-mms.com)"

	exit 1
fi

tsvPathName=$1
userKey=$2
apiKey=$3
mmsApiHostname=$4

awk -v userKey=$userKey -v apiKey=$apiKey -v mmsApiHostname=$mmsApiHostname -f ./utility/serieUpdate.awk $tsvPathName

