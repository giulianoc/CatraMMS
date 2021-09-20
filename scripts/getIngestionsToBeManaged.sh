#!/bin/bash

if [ $# -ne 1 ];
then
	echo "Usage $0 <dbPassword>"

	exit 1
fi

dbPassword=$1


echo "dbPassword: $dbPassword"

#this select is the one avoiding Live-Recorder, Live-Proxy and VOD-Proxy
echo "select ij.ingestionJobKey, ij.label, ir.workspaceKey, ij.status, ij.ingestionType, ingestionDate
from MMS_IngestionRoot ir, MMS_IngestionJob ij
where ir.ingestionRootKey = ij.ingestionRootKey and ij.processorMMS is null
and (ij.ingestionType != 'Live-Recorder' and ij.ingestionType != 'Live-Proxy' and ij.ingestionType != 'VOD-Proxy')
and (ij.status = 'Start_TaskQueued' or (ij.status in ('SourceDownloadingInProgress', 'SourceMovingInProgress', 'SourceCopingInProgress', 'SourceUploadingInProgress') and ij.sourceBinaryTransferred = 1))
and ij.processingStartingFrom <= NOW() and NOW() <= DATE_ADD(ij.processingStartingFrom, INTERVAL 7 DAY)
order by ij.priority asc, ij.processingStartingFrom asc" | mysql -N -u mms -p$dbPassword -h db-server-active mms

