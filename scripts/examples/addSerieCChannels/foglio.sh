#!/bin/bash                                                                                                   

if [ $# -ne 4 ];
then
	echo "Usage $0 <tsv path name> <userKey> <apiKey> <mmsAPIHostname> (i.e.: 40 702347803978348... mms-api.cloud-mms.com)"

	exit 1
fi

tsvPathName=$1

#for seriec: 1
userKey=$2

#for seriec: 1j1f1C1f1l1e1r1u1w1y111f1r1p1b1b1V1H1S1b1b191418091909170916
apiKey=$3

#for seriec: mms-api.restream.ovh
mmsApiHostname=$4

awk -v userKey=$userKey -v apiKey=$apiKey -v mmsApiHostname=$mmsApiHostname -f ./utility/seriec.awk $tsvPathName

