#!/bin/bash


echo "mmsApi"
/opt/catramms/CatraMMS/scripts/mmsApi.rsi.lab.sh stop

echo "mmsEncoder"
/opt/catramms/CatraMMS/scripts/mmsEncoder.rsi.lab.sh stop

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.lab.sh stop

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh stop

echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh stop

date
