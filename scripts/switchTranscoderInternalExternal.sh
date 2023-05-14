#!/bin/bash

if [ $# -ne 1 ];
then
	echo "Usage $0 <internal or external>"

	exit 1
fi

if [ "$1" == "internal" ];
then
	rm ~/mms/conf/catramms.nginx; ln -s ~/mms/conf/catramms.nginx.internal ~/mms/conf/catramms.nginx
	rm ~/mms/conf/crontab.txt; ln -s ~/mms/conf/crontab.txt.internal ~/mms/conf/crontab.txt
	rm ~/mms/conf/mms.cfg ; ln -s /var/catramms/storage/commonConfiguration/mms.cfg ~/mms/conf/mms.cfg
else
	rm ~/mms/conf/catramms.nginx; ln -s ~/mms/conf/catramms.nginx.external ~/mms/conf/catramms.nginx
	rm ~/mms/conf/crontab.txt; ln -s ~/mms/conf/crontab.txt.external ~/mms/conf/crontab.txt
	rm ~/mms/conf/mms.cfg; ln -s /var/catramms/storage/commonConfiguration/external-mms-for-aws.cfg ~/mms/conf/mms.cfg
fi

~/mmsStopALL.sh
sleep 5
~/mmsStatusALL.sh
~/mmsStartALL.sh
crontab ~/mms/conf/crontab.txt
#The  process  name used for matching is limited to the 15 characters
pkill loopServicesS

