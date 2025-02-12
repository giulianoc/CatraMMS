#!/bin/bash

if [ $# -lt 6 -o $# -gt 8 ]; then
	echo "Usage: $0 <mmsUserKey> <mmsAPIKey> <title> <tag> <uniqueName> <ingester> [<retention> <fileFormat>]"

	exit 1
fi

mmsAPIHostName=mms-api.catramms-cloud.com

mmsUserKey=$1
mmsAPIKey=$2
title=$3
tag=$4
uniqueName=$5
ingester=$6
if [ $# -gt 6 ]; then
	retention=$7
else
	retention="3d"
fi
if [ $# -gt 7 ]; then
	fileFormat=$8
else
	fileFormat="mp4"
fi

scriptPathName=$(readlink -f "$0")
scriptDirectory=$(dirname "$scriptPathName")

#if ! command -v jq &> /dev/null
if ! [ -x "$(command -v jq)" ]
then
    echo "The 'jq' command could not be found, please install it"

    exit
fi

sed "s/\${title}/$title/g" $scriptDirectory/ingestionWorkflow.json | sed "s/\${tag}/$tag/g" | sed "s/\${uniqueName}/$uniqueName/g" | sed "s/\${ingester}/$ingester/g" | sed "s/\${retention}/$retention/g" | sed "s/\${fileFormat}/$fileFormat/g" > $scriptDirectory/ingestionWorkflow.json.new

curl -o $scriptDirectory/ingestionWorkflowResult.json -k -s -X POST -u $mmsUserKey:$mmsAPIKey -d @$scriptDirectory/ingestionWorkflow.json.new -H "Content-Type: application/json" https://$mmsAPIHostName/catramms/1.0.1/workflow

rm $scriptDirectory/ingestionWorkflow.json.new

#print ingestionJobKey
jq '.tasks[] | select(.type == "Add-Content") | .ingestionJobKey' $scriptDirectory/ingestionWorkflowResult.json

#2022-06-29: not removed because printed by the caller script in case of error
#	The file is also removed by the caller script
#rm ./helper/ingestionWorkflowResult.json


