#!/bin/bash


echo "api"
/opt/catramms/CatraMMS/scripts/api.sh stop

echo "encoder"
/opt/catramms/CatraMMS/scripts/encoder.sh stop

echo "mmsEngineService"
/opt/catramms/CatraMMS/scripts/mmsEngineService.sh stop

echo "tomcat"
/opt/catramms/CatraMMS/scripts/tomcat.sh stop

echo "nginx"
/opt/catramms/CatraMMS/scripts/nginx.sh start

date
