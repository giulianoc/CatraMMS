#!/bin/bash                                                                                                   

if [ $# -ne 4 ];
then
	echo "Usage $0 <tsv path name> <userKey> <apiKey> <mmsAPIHostname> (i.e.: 40 702347803978348... mms-api.cloud-mms.com)"

	exit 1
fi

tsvPathName=$1

#for cibortv: 1
userKey=$2

#for cibortv: 1j1l1x1o1l1d1q1r1f1d1w1u1d1p1e1r1q1h1C1j1p1d1l1o111f1r1p1b1b1V1H1S1b1b1:1:0817191313151:
apiKey=$3

#for cibortv: mms-api.restream.ovh
mmsApiHostname=$4

awk -v userKey=$userKey -v apiKey=$apiKey -v mmsApiHostname=$mmsApiHostname -f ./utility/movie.awk $tsvPathName

