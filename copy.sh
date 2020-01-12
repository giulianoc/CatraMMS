#!/bin/bash
  
date

currentDir=$(pwd)

cd /opt/catrasoftware/deploy
tar cvfz CatraMMS-ubuntu-18.04.tar.gz CatraMMS
cd $currentDir

echo -n "deploy su cibor? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-api-gui-1:/opt/catramms
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-engine-2:/opt/catramms
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-engine-db-1:/opt/catramms
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-transcoder-1:/opt/catramms
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-transcoder-ita-1:/opt/catramms
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@cibortv-mms-transcoder-ita-2:/opt/catramms
fi

echo -n "deploy su mms cloud? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp -P 9255 /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@mms-engine-1:/opt/catramms
fi

echo -n "deploy su rsis-lab-mmst? " 
read deploy
if [ "$deploy" == "y" ]; then
	scp /opt/catrasoftware/deploy/CatraMMS-ubuntu-18.04.tar.gz mms@rsis-lab-mmst:/opt/catramms
fi


