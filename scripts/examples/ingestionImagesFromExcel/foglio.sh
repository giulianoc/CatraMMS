#!/bin/bash                                                                                                   

if [ $# -ne 2 ];
then
	echo "Usage $0 <userKey> <apiKey>>"

	exit 1
fi

userKey=$1
apiKey=$2

awk -v userKey=$userKey -v apiKey=$apiKey -f ./utility/foglio.awk foglio.tsv

