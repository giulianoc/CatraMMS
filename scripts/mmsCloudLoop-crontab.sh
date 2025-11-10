#!/bin/bash

source ~/mms/conf/mms-env.sh

if [ ! -f "$debugFilename" ]; then
	echo "" > $debugFilename
else
	filesize=$(stat -c %s $debugFilename)
	if [ $filesize -gt 10000000 ]
	then
		echo "" > $debugFilename
	fi
fi

pgrep -f "mmsCloudLoop.sh prod" > /dev/null
if [ $? -ne 0 ]; then
  TELEGRAM_GROUPALARMS_BOT_TOKEN=$MMS_TELEGRAM_GROUPALARMS_BOT_TOKEN_PROD TELEGRAM_GROUPALARMS_ID=$MMS_TELEGRAM_GROUPALARMS_ID_PROD nohup /opt/catrasoftware/CatraMMS/scripts/mmsCloudLoop.sh prod > /dev/null &
fi;

pgrep -f "mmsCloudLoop.sh test" > /dev/null
if [ $? -ne 0 ]; then
  TELEGRAM_GROUPALARMS_BOT_TOKEN=$MMS_TELEGRAM_GROUPALARMS_BOT_TOKEN_TEST TELEGRAM_GROUPALARMS_ID=$MMS_TELEGRAM_GROUPALARMS_ID_TEST nohup /opt/catrasoftware/CatraMMS/scripts/mmsCloudLoop.sh test > /dev/null &
fi;

