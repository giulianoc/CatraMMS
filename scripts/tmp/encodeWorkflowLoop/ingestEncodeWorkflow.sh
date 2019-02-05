#!/bin/bash



physicalPathKeyStart=8846
physicalPathKeyEnd=8881
while [ $physicalPathKeyStart -le $physicalPathKeyEnd ]
do
	jsonPathName=./tmp/$physicalPathKeyStart.json

	echo "$jsonPathName"

	sed s/__REFERENCEPHYSICALPATHKEY__/$physicalPathKeyStart/ encodeWorkflowTemplate.json > $jsonPathName
	curl -k -v -X POST -u 1:SU1.8ZO1O2zTg_5SvI12rfN9oQdjRru90XbMRSvACIxf6iNunYz7nzLF0ZVfaeCChP -d @$jsonPathName -H "Content-Type: application/json" https://mms-api.catrasoft.cloud/catramms/v1/ingestion

	rm -rf $jsonPathName

	(( physicalPathKeyStart++ ))
done

