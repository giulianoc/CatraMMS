#!/bin/bash

echo "chmod .sh"
chmod u+x /opt/catramms/CatraMMS/etc/*.sh

echo "crontab"
crontab -u mms /opt/catramms/CatraMMS/etc/crontab.txt

echo "nginx"
/opt/catramms/CatraMMS/etc/nginx.sh stop
/opt/catramms/CatraMMS/etc/nginx.sh status
/opt/catramms/CatraMMS/etc/nginx.sh start

echo "api"
/opt/catramms/CatraMMS/etc/api.rsi.sh stop
/opt/catramms/CatraMMS/etc/api.rsi.sh status
/opt/catramms/CatraMMS/etc/api.rsi.sh start

echo "mmsEngineService"
/opt/catramms/CatraMMS/etc/mmsEngineService.rsi.sh stop
/opt/catramms/CatraMMS/etc/mmsEngineService.rsi.sh status
/opt/catramms/CatraMMS/etc/mmsEngineService.rsi.sh start

echo "encoder"
/opt/catramms/CatraMMS/etc/encoder.rsi.sh stop
/opt/catramms/CatraMMS/etc/encoder.rsi.sh status
/opt/catramms/CatraMMS/etc/encoder.rsi.sh start

echo "tomcat"
/opt/catramms/CatraMMS/etc/tomcat.sh stop
/opt/catramms/CatraMMS/etc/tomcat.sh status
/opt/catramms/CatraMMS/etc/tomcat.sh start

