#!/bin/bash


echo "api"
/opt/catramms/CatraMMS/scripts/api.rsi.sh stop

echo "encoder"
/opt/catramms/CatraMMS/scripts/encoder.rsi.sh stop

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.rsi.sh stop

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh stop

echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh stop

date
