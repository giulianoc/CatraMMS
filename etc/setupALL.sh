#!/bin/bash

echo "chmod .sh"
chmod u+x /home/mms/catramms/CatraMMS/etc/*.sh

echo "crontab"
crontab -u mms /home/mms/catramms/CatraMMS/etc/crontab.txt

echo "nginx"
/home/mms/nginx.sh stop
/home/mms/nginx.sh status
/home/mms/nginx.sh start

echo "api"
/home/mms/api.sh stop
/home/mms/api.sh status
/home/mms/api.sh start

echo "mmsEngineService"
/home/mms/mmsEngineService.sh stop
/home/mms/mmsEngineService.sh status
/home/mms/mmsEngineService.sh start

echo "api"
/home/mms/encoder.sh stop
/home/mms/encoder.sh status
/home/mms/encoder.sh start

echo "tomcat"
/home/mms/tomcat.sh stop
/home/mms/tomcat.sh status
/home/mms/tomcat.sh start

